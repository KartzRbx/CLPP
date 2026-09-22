//! `clpp lsp` — protocol via [`tower_lsp_server`]; semantics via [`crate::analysis`] / Session.

use crate::analysis::{
    complete_request, definition_request, hover_request, PositionRequest,
};
use std::collections::HashMap;
use std::io;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use tokio::sync::Mutex;
use tower_lsp_server::jsonrpc::Result as LspResult;
use tower_lsp_server::ls_types::*;
use tower_lsp_server::{Client, LanguageServer, LspService, Server};

struct Doc {
    text: String,
}

struct State {
    docs: HashMap<Uri, Doc>,
    gen: HashMap<Uri, u64>,
    session: crate::session::Session,
}

pub struct Backend {
    client: Client,
    state: Arc<Mutex<State>>,
    /// Monotonic id so delayed diagnostic publishes can cancel themselves.
    tick: AtomicU64,
}

pub fn run() -> io::Result<()> {
    let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()?;
    rt.block_on(run_async())
}

async fn run_async() -> io::Result<()> {
    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();
    let (service, socket) = LspService::new(|client| Backend {
        client,
        state: Arc::new(Mutex::new(State {
            docs: HashMap::new(),
            gen: HashMap::new(),
            session: crate::session::Session::new(),
        })),
        tick: AtomicU64::new(0),
    });
    Server::new(stdin, stdout, socket).serve(service).await;
    Ok(())
}

impl LanguageServer for Backend {
    async fn initialize(&self, _: InitializeParams) -> LspResult<InitializeResult> {
        Ok(InitializeResult {
            capabilities: ServerCapabilities {
                text_document_sync: Some(TextDocumentSyncCapability::Kind(
                    TextDocumentSyncKind::FULL,
                )),
                completion_provider: Some(CompletionOptions {
                    trigger_characters: Some(vec![
                        ".".into(),
                        "@".into(),
                        "<".into(),
                        "(".into(),
                        ":".into(),
                        "~".into(),
                    ]),
                    ..Default::default()
                }),
                hover_provider: Some(HoverProviderCapability::Simple(true)),
                definition_provider: Some(OneOf::Left(true)),
                ..Default::default()
            },
            server_info: Some(ServerInfo {
                name: "clpp".into(),
                version: Some(env!("CARGO_PKG_VERSION").into()),
            }),
            ..Default::default()
        })
    }

    async fn shutdown(&self) -> LspResult<()> {
        Ok(())
    }

    async fn did_open(&self, params: DidOpenTextDocumentParams) {
        let uri = params.text_document.uri;
        let text = params.text_document.text;
        {
            let mut st = self.state.lock().await;
            st.docs.insert(uri.clone(), Doc { text });
            st.gen.insert(uri.clone(), 1);
        }
        self.publish_diagnostics(&uri).await;
    }

    async fn did_change(&self, params: DidChangeTextDocumentParams) {
        let uri = params.text_document.uri;
        let text = params
            .content_changes
            .into_iter()
            .next()
            .map(|c| c.text)
            .unwrap_or_default();
        let gen = {
            let mut st = self.state.lock().await;
            st.docs.insert(uri.clone(), Doc { text });
            let g = st.gen.entry(uri.clone()).or_insert(0);
            *g += 1;
            *g
        };
        let tick = self.tick.fetch_add(1, Ordering::SeqCst) + 1;
        let client = self.client.clone();
        let state = Arc::clone(&self.state);
        let uri_clone = uri.clone();
        tokio::spawn(async move {
            tokio::time::sleep(std::time::Duration::from_millis(40)).await;
            let current = state
                .lock()
                .await
                .gen
                .get(&uri_clone)
                .copied()
                .unwrap_or(0);
            if current != gen {
                return;
            }
            let _ = tick;
            let diagnostics = collect_diagnostics(&state, &uri_clone).await;
            client
                .publish_diagnostics(uri_clone, diagnostics, None)
                .await;
        });
    }

    async fn did_close(&self, params: DidCloseTextDocumentParams) {
        let mut st = self.state.lock().await;
        st.docs.remove(&params.text_document.uri);
        st.gen.remove(&params.text_document.uri);
    }

    async fn completion(&self, params: CompletionParams) -> LspResult<Option<CompletionResponse>> {
        let uri = &params.text_document_position.text_document.uri;
        let text = doc_text(&self.state, uri).await;
        let req = pos_req(uri, text, &params.text_document_position.position);
        let resp = complete_request(&req);
        let items: Vec<CompletionItem> = resp
            .items
            .into_iter()
            .map(|item| CompletionItem {
                label: item.label,
                kind: Some(CompletionItemKind::VARIABLE),
                detail: Some(item.detail),
                insert_text: item.insert_text,
                insert_text_format: Some(InsertTextFormat::SNIPPET),
                ..Default::default()
            })
            .collect();
        Ok(Some(CompletionResponse::List(CompletionList {
            is_incomplete: false,
            items,
        })))
    }

    async fn hover(&self, params: HoverParams) -> LspResult<Option<Hover>> {
        let uri = &params.text_document_position_params.text_document.uri;
        let text = doc_text(&self.state, uri).await;
        let req = pos_req(uri, text, &params.text_document_position_params.position);
        Ok(hover_request(&req).hover.map(|h| Hover {
            contents: HoverContents::Markup(MarkupContent {
                kind: MarkupKind::Markdown,
                value: h.contents,
            }),
            range: None,
        }))
    }

    async fn goto_definition(
        &self,
        params: GotoDefinitionParams,
    ) -> LspResult<Option<GotoDefinitionResponse>> {
        let uri = &params.text_document_position_params.text_document.uri;
        let text = doc_text(&self.state, uri).await;
        let req = pos_req(uri, text, &params.text_document_position_params.position);
        let locs = definition_request(&req).locations;
        if locs.is_empty() {
            return Ok(None);
        }
        let locations: Vec<Location> = locs
            .into_iter()
            .map(|loc| {
                let line = loc.line.saturating_sub(1) as u32;
                let col = loc.column.saturating_sub(1) as u32;
                Location {
                    uri: uri.clone(),
                    range: Range {
                        start: Position {
                            line,
                            character: col,
                        },
                        end: Position {
                            line,
                            character: col,
                        },
                    },
                }
            })
            .collect();
        Ok(Some(GotoDefinitionResponse::Array(locations)))
    }
}

impl Backend {
    async fn publish_diagnostics(&self, uri: &Uri) {
        let diagnostics = collect_diagnostics(&self.state, uri).await;
        self.client
            .publish_diagnostics(uri.clone(), diagnostics, None)
            .await;
    }
}

async fn doc_text(state: &Mutex<State>, uri: &Uri) -> String {
    state
        .lock()
        .await
        .docs
        .get(uri)
        .map(|d| d.text.clone())
        .unwrap_or_default()
}

fn pos_req(uri: &Uri, text: String, position: &Position) -> PositionRequest {
    PositionRequest {
        source: text,
        file_name: uri_to_path(uri),
        line: position.line as usize + 1,
        column: position.character as usize + 1,
    }
}

fn uri_to_path(uri: &Uri) -> String {
    let s = uri.as_str();
    let stripped = s
        .strip_prefix("file:///")
        .or_else(|| s.strip_prefix("file://"))
        .unwrap_or(s);
    if cfg!(windows) {
        stripped.replace('/', "\\")
    } else {
        stripped.to_string()
    }
}

async fn collect_diagnostics(state: &Mutex<State>, uri: &Uri) -> Vec<Diagnostic> {
    let text = doc_text(state, uri).await;
    let path = uri_to_path(uri);
    let mut st = state.lock().await;
    let mut api = crate::session::CompilerApi::new(&mut st.session);
    let _ = api.check(&text, &path);
    api.get_diagnostics(&path)
        .iter()
        .map(|d| {
            let line = d.line.saturating_sub(1) as u32;
            let col = d.column.saturating_sub(1) as u32;
            Diagnostic {
                range: Range {
                    start: Position {
                        line,
                        character: col,
                    },
                    end: Position {
                        line,
                        character: col + 1,
                    },
                },
                severity: Some(if d.severity == "warning" {
                    DiagnosticSeverity::WARNING
                } else {
                    DiagnosticSeverity::ERROR
                }),
                code: d.code.clone().map(NumberOrString::String),
                source: Some("clpp".into()),
                message: d.message.clone(),
                ..Default::default()
            }
        })
        .collect()
}
