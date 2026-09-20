//! Persistent JSON-RPC language server (`clpp lsp`).

use crate::analysis::{
    complete_request, definition_request, hover_request, PositionRequest,
};
use crate::compile::compile_artifact_source;
use serde_json::{json, Value};
use std::collections::HashMap;
use std::io::{self, BufRead, Write};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

struct Doc {
    text: String,
}

struct State {
    docs: HashMap<String, Doc>,
    gen: HashMap<String, u64>,
}

pub fn run() -> io::Result<()> {
    let state = Arc::new(Mutex::new(State {
        docs: HashMap::new(),
        gen: HashMap::new(),
    }));
    let stdin = io::stdin();
    let stdout = Arc::new(Mutex::new(io::stdout()));
    let mut reader = stdin.lock();
    loop {
        let Some(msg) = read_message(&mut reader)? else {
            break;
        };
        let parsed: Value = match serde_json::from_slice(&msg) {
            Ok(v) => v,
            Err(_) => continue,
        };
        let id = parsed.get("id").cloned();
        let method = parsed
            .get("method")
            .and_then(|m| m.as_str())
            .unwrap_or("")
            .to_string();
        let params = parsed.get("params").cloned().unwrap_or(Value::Null);
        if method == "exit" {
            break;
        }
        let state_ref = Arc::clone(&state);
        let result = catch_unwind(AssertUnwindSafe(move || dispatch(&state_ref, &method, params)));
        match result {
            Ok(Dispatch::Notify) => {}
            Ok(Dispatch::Reply(value)) => {
                if let Some(id) = id {
                    let mut out = stdout.lock().unwrap();
                    write_message(&mut *out, json!({"jsonrpc":"2.0","id":id,"result":value}))?;
                }
            }
            Ok(Dispatch::Diagnostics { uri, diagnostics }) => {
                let mut out = stdout.lock().unwrap();
                write_message(
                    &mut *out,
                    json!({
                        "jsonrpc": "2.0",
                        "method": "textDocument/publishDiagnostics",
                        "params": { "uri": uri, "diagnostics": diagnostics }
                    }),
                )?;
            }
            Ok(Dispatch::Debounce { uri, gen }) => {
                let state = Arc::clone(&state);
                let stdout = Arc::clone(&stdout);
                thread::spawn(move || {
                    thread::sleep(Duration::from_millis(40));
                    let current = state
                        .lock()
                        .ok()
                        .and_then(|st| st.gen.get(&uri).copied())
                        .unwrap_or(0);
                    if current != gen {
                        return;
                    }
                    let diagnostics = publish_diags(&state, &uri);
                    if let Ok(mut out) = stdout.lock() {
                        let _ = write_message(
                            &mut *out,
                            json!({
                                "jsonrpc": "2.0",
                                "method": "textDocument/publishDiagnostics",
                                "params": { "uri": uri, "diagnostics": diagnostics }
                            }),
                        );
                    }
                });
            }
            Err(_) => {
                if let Some(id) = id {
                    let mut out = stdout.lock().unwrap();
                    write_message(
                        &mut *out,
                        json!({
                            "jsonrpc":"2.0",
                            "id": id,
                            "error": { "code": -32603, "message": "internal error" }
                        }),
                    )?;
                }
            }
        }
    }
    Ok(())
}

enum Dispatch {
    Notify,
    Reply(Value),
    Diagnostics { uri: String, diagnostics: Value },
    Debounce { uri: String, gen: u64 },
}

fn dispatch(state: &Arc<Mutex<State>>, method: &str, params: Value) -> Dispatch {
    match method {
        "initialize" => Dispatch::Reply(json!({
            "capabilities": {
                "textDocumentSync": 1,
                "completionProvider": { "triggerCharacters": [".", "@", "<", "(", ":", "~"] },
                "hoverProvider": true,
                "definitionProvider": true
            },
            "serverInfo": { "name": "clpp", "version": env!("CARGO_PKG_VERSION") }
        })),
        "initialized" | "shutdown" | "textDocument/didClose" => Dispatch::Notify,
        "textDocument/didOpen" => {
            let uri = params
                .pointer("/textDocument/uri")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let text = params
                .pointer("/textDocument/text")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            {
                let mut st = state.lock().unwrap();
                st.docs.insert(uri.clone(), Doc { text });
                st.gen.insert(uri.clone(), 1);
            }
            let diagnostics = publish_diags(state, &uri);
            Dispatch::Diagnostics {
                uri,
                diagnostics,
            }
        }
        "textDocument/didChange" => {
            let uri = params
                .pointer("/textDocument/uri")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let text = params
                .pointer("/contentChanges/0/text")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let gen = {
                let mut st = state.lock().unwrap();
                st.docs.insert(uri.clone(), Doc { text });
                let g = st.gen.entry(uri.clone()).or_insert(0);
                *g += 1;
                *g
            };
            Dispatch::Debounce { uri, gen }
        }
        "textDocument/completion" => Dispatch::Reply(completion(state, &params)),
        "textDocument/hover" => Dispatch::Reply(hover(state, &params)),
        "textDocument/definition" => Dispatch::Reply(definition(state, &params)),
        _ => Dispatch::Reply(Value::Null),
    }
}

fn doc_text(state: &Arc<Mutex<State>>, uri: &str) -> String {
    state
        .lock()
        .unwrap()
        .docs
        .get(uri)
        .map(|d| d.text.clone())
        .unwrap_or_default()
}

fn pos_req(uri: &str, text: String, params: &Value) -> PositionRequest {
    let line = params
        .pointer("/position/line")
        .and_then(|v| v.as_u64())
        .unwrap_or(0) as usize
        + 1;
    let column = params
        .pointer("/position/character")
        .and_then(|v| v.as_u64())
        .unwrap_or(0) as usize
        + 1;
    PositionRequest {
        source: text,
        file_name: uri_to_path(uri),
        line,
        column,
    }
}

fn uri_to_path(uri: &str) -> String {
    let stripped = uri
        .strip_prefix("file:///")
        .or_else(|| uri.strip_prefix("file://"))
        .unwrap_or(uri);
    if cfg!(windows) {
        stripped.replace('/', "\\")
    } else {
        stripped.to_string()
    }
}

fn completion(state: &Arc<Mutex<State>>, params: &Value) -> Value {
    let uri = params
        .pointer("/textDocument/uri")
        .and_then(|v| v.as_str())
        .unwrap_or("");
    let text = doc_text(state, uri);
    let req = pos_req(uri, text, params);
    let resp = complete_request(&req);
    let items: Vec<Value> = resp
        .items
        .into_iter()
        .map(|item| {
            json!({
                "label": item.label,
                "kind": 6,
                "detail": item.detail,
                "insertText": item.insert_text,
                "insertTextFormat": 2
            })
        })
        .collect();
    json!({ "isIncomplete": false, "items": items })
}

fn hover(state: &Arc<Mutex<State>>, params: &Value) -> Value {
    let uri = params
        .pointer("/textDocument/uri")
        .and_then(|v| v.as_str())
        .unwrap_or("");
    let text = doc_text(state, uri);
    let req = pos_req(uri, text, params);
    match hover_request(&req).hover {
        Some(h) => json!({ "contents": { "kind": "markdown", "value": h.contents } }),
        None => Value::Null,
    }
}

fn definition(state: &Arc<Mutex<State>>, params: &Value) -> Value {
    let uri = params
        .pointer("/textDocument/uri")
        .and_then(|v| v.as_str())
        .unwrap_or("");
    let text = doc_text(state, uri);
    let req = pos_req(uri, text, params);
    let locs = definition_request(&req).locations;
    if locs.is_empty() {
        return Value::Null;
    }
    json!(locs
        .into_iter()
        .map(|loc| json!({
            "uri": uri,
            "range": {
                "start": { "line": loc.line.saturating_sub(1), "character": loc.column.saturating_sub(1) },
                "end": { "line": loc.line.saturating_sub(1), "character": loc.column.saturating_sub(1) }
            }
        }))
        .collect::<Vec<_>>())
}

fn publish_diags(state: &Arc<Mutex<State>>, uri: &str) -> Value {
    let text = doc_text(state, uri);
    let path = PathBuf::from(uri_to_path(uri));
    let art = compile_artifact_source(&text, &path, None).unwrap_or_else(|err| {
        crate::support::CompileArtifact::fail(path.display().to_string(), format!("{err:#}"))
    });
    let diags: Vec<Value> = art
        .diagnostics
        .into_iter()
        .map(|d| {
            let line = d.line.saturating_sub(1);
            let col = d.column.saturating_sub(1);
            json!({
                "range": {
                    "start": { "line": line, "character": col },
                    "end": { "line": line, "character": col + 1 }
                },
                "severity": if d.severity == "warning" { 2 } else { 1 },
                "code": d.code,
                "source": "clpp",
                "message": d.message
            })
        })
        .collect();
    Value::Array(diags)
}

fn read_message(reader: &mut impl BufRead) -> io::Result<Option<Vec<u8>>> {
    let mut content_length = None;
    loop {
        let mut header = String::new();
        let n = reader.read_line(&mut header)?;
        if n == 0 {
            return Ok(None);
        }
        let header = header.trim_end();
        if header.is_empty() {
            break;
        }
        if let Some(rest) = header.strip_prefix("Content-Length:") {
            content_length = rest.trim().parse::<usize>().ok();
        }
    }
    let len = match content_length {
        Some(n) => n,
        None => return Ok(None),
    };
    let mut buf = vec![0u8; len];
    reader.read_exact(&mut buf)?;
    Ok(Some(buf))
}

fn write_message(out: &mut impl Write, value: Value) -> io::Result<()> {
    let body = serde_json::to_vec(&value)?;
    write!(out, "Content-Length: {}\r\n\r\n", body.len())?;
    out.write_all(&body)?;
    out.flush()
}

/// Debounce helper used by the editor host: cancel previous work by dropping the flag.
pub fn debounce_ms(ms: u64, flag: Arc<AtomicBool>, work: impl FnOnce() + Send + 'static) {
    flag.store(true, Ordering::SeqCst);
    let mine = Arc::new(AtomicBool::new(true));
    let keep = Arc::clone(&mine);
    thread::spawn(move || {
        thread::sleep(Duration::from_millis(ms));
        if keep.load(Ordering::SeqCst) && flag.load(Ordering::SeqCst) {
            work();
        }
    });
    let _ = flag;
}
