//! New front-end: parse → `clpp_ty` → `clpp_codegen`.
//! The binary name is `clpp_cli` until the root `clpp` binary (still on Pest) is retired.

pub mod path_resolver;

use clpp_codegen::{emit, EmitError};
use clpp_parser::{parse, USE_LINK};
use clpp_ty::RunContext;
use std::path::Path;

pub fn run_context_from_path(path: &Path) -> RunContext {
    let name = path.file_name().and_then(|s| s.to_str()).unwrap_or("");
    if name.contains(".client.") {
        RunContext::Client
    } else if name.contains(".server.") {
        RunContext::Server
    } else {
        RunContext::Module
    }
}

/// Parser diagnostics, then AUTH/PAR, then Luau. Safety errors produce no Luau.
pub fn build_source(source: &str, ctx: RunContext) -> Result<String, String> {
    let parsed = parse(source);
    if parsed.errors.iter().any(|e| e.message == USE_LINK) {
        return Err(USE_LINK.into());
    }
    match emit(source, ctx) {
        Ok(luau) => Ok(luau),
        Err(EmitError::Blocked(diags)) => Err(diags
            .iter()
            .map(|d| format!("{}:{}: {}", d.code, d.line, d.message))
            .collect::<Vec<_>>()
            .join("\n")),
    }
}

pub fn build_file(path: &Path) -> Result<String, String> {
    let source = std::fs::read_to_string(path).map_err(|e| e.to_string())?;
    build_source(&source, run_context_from_path(path))
}

/// One-shot diagnostic pass (the stdio server remains the root `clpp lsp` until cutover).
pub fn check_file(path: &Path) -> Result<(), String> {
    let source = std::fs::read_to_string(path).map_err(|e| e.to_string())?;
    let ctx = run_context_from_path(path);
    let parsed = parse(&source);
    let mut messages: Vec<String> = parsed
        .errors
        .iter()
        .map(|e| format!("parse: {}", e.message))
        .collect();
    let checked = clpp_ty::check(&source, ctx);
    messages.extend(checked.diagnostics.iter().map(|d| format!("{}:{}: {}", d.code, d.line, d.message)));
    if messages.is_empty() {
        Ok(())
    } else {
        Err(messages.join("\n"))
    }
}

pub fn run<I, S>(args: I) -> i32
where
    I: IntoIterator<Item = S>,
    S: AsRef<str>,
{
    let args: Vec<String> = args.into_iter().map(|s| s.as_ref().to_string()).collect();
    let Some(cmd) = args.first() else {
        eprintln!("usage: clpp_cli <build|lsp> <file.clpp>");
        return 2;
    };
    let Some(file) = args.get(1) else {
        eprintln!("usage: clpp_cli {cmd} <file.clpp>");
        return 2;
    };
    let path = Path::new(file);
    let sourcemap = args.iter().any(|a| a == "--sourcemap");
    match dispatch(cmd, path, file, sourcemap) {
        Ok(()) => 0,
        Err(err) => {
            eprintln!("{err}");
            1
        }
    }
}

fn dispatch(cmd: &str, path: &Path, file: &str, sourcemap: bool) -> Result<(), String> {
    let source = std::fs::read_to_string(path).map_err(|e| e.to_string())?;
    match cmd {
        "build" if sourcemap => {
            let mapped = clpp_codegen::emit_mapped(&source, run_context_from_path(path), file).map_err(|e| match e {
                clpp_codegen::EmitError::Blocked(diags) => diags
                    .iter()
                    .map(|d| format!("{}:{}: {}", d.code, d.line, d.message))
                    .collect::<Vec<_>>()
                    .join("\n"),
            })?;
            println!("{}", mapped.luau);
            eprintln!("{}", mapped.sourcemap);
            Ok(())
        }
        "build" => {
            print!("{}", build_source(&source, run_context_from_path(path))?);
            Ok(())
        }
        "lsp" => check_source(&source, run_context_from_path(path)),
        "lint" => {
            let found = clpp_ty::lint(&source);
            if found.is_empty() {
                Ok(())
            } else {
                Err(found.iter().map(|d| format!("{}:{}: {}", d.code, d.line, d.message)).collect::<Vec<_>>().join("\n"))
            }
        }
        "fmt" => {
            print!("{}", fmt_source(&source));
            Ok(())
        }
        "doc" => {
            print!("{}", doc_source(&source));
            Ok(())
        }
        "update-types" => {
            print!("{}", headers_from_dump(&source));
            Ok(())
        }
        _ => Err(format!("unknown command `{cmd}`")),
    }
}

fn check_source(source: &str, ctx: RunContext) -> Result<(), String> {
    let parsed = parse(source);
    let mut messages: Vec<String> = parsed.errors.iter().map(|e| format!("parse: {}", e.message)).collect();
    messages.extend(clpp_ty::check(source, ctx).diagnostics.iter().map(|d| format!("{}:{}: {}", d.code, d.line, d.message)));
    if messages.is_empty() { Ok(()) } else { Err(messages.join("\n")) }
}

pub fn fmt_source(source: &str) -> String {
    let mut out = String::new();
    for line in source.lines() {
        let t = line.trim();
        if let Some(rest) = t.strip_prefix("link ") {
            let rest = rest.split_whitespace().collect::<Vec<_>>().join(" ");
            out.push_str("link ");
            out.push_str(&rest);
            if !rest.ends_with(';') {
                out.push(';');
            }
            out.push('\n');
        } else {
            out.push_str(t);
            out.push('\n');
        }
    }
    out
}

pub fn doc_source(source: &str) -> String {
    let lines: Vec<&str> = source.lines().collect();
    let mut out = String::from("# API\n\n");
    for (i, line) in lines.iter().enumerate() {
        let t = line.trim();
        if let Some(name) = t.strip_prefix("struct ") {
            let name = name.split([' ', '{']).next().unwrap_or(name);
            let doc = lines.get(i.wrapping_sub(1)).and_then(|p| p.trim().strip_prefix("///")).unwrap_or("");
            out.push_str(&format!("## {name}\n\n{doc}\n\n"));
        }
    }
    out
}

pub fn headers_from_dump(json: &str) -> String {
    let value: serde_json::Value = serde_json::from_str(json).unwrap_or(serde_json::Value::Null);
    let mut out = String::from("#pragma once\n");
    let classes = value.get("Classes").and_then(|c| c.as_array());
    for class in classes.into_iter().flatten() {
        let Some(name) = class.get("Name").and_then(|n| n.as_str()) else { continue };
        out.push_str(&format!("struct {name} {{\n"));
        if let Some(members) = class.get("Members").and_then(|m| m.as_array()) {
            for member in members {
                if member.get("MemberType").and_then(|t| t.as_str()) != Some("Method") {
                    continue;
                }
                if let Some(method) = member.get("Name").and_then(|n| n.as_str()) {
                    out.push_str(&format!("    void {method}();\n"));
                }
            }
        }
        out.push_str("};\n");
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tags_choose_context() {
        assert_eq!(run_context_from_path(Path::new("A.client.clpp")), RunContext::Client);
        assert_eq!(run_context_from_path(Path::new("A.server.clpp")), RunContext::Server);
        assert_eq!(run_context_from_path(Path::new("A.clpp")), RunContext::Module);
    }

    #[test]
    fn client_server_call_blocks_build() {
        let src = "@server\nvoid Save();\nvoid init() { Save(); }\n";
        let err = build_source(src, RunContext::Client).unwrap_err();
        assert!(err.contains("CLUAU_AUTH001"));
    }

    #[test]
    fn game_link_builds_on_server() {
        let src = "link @game.ReplicatedStorage.Modules.Combat as CombatModule;\n";
        let luau = build_source(src, RunContext::Server).unwrap();
        assert!(luau.contains("GetService"));
    }
}
