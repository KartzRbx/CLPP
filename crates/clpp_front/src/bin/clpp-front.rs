//! Dump tokens, the lossless tree, or the typed AST.
//!
//! ```text
//! clpp-front tokens <file>
//! clpp-front tree <file>
//! clpp-front ast <file>
//! clpp-front check <file>
//! ```
//!
//! `-` reads stdin. Exit status is 1 when the file has errors.

use clpp_front::{dump_ast, dump_cst, lex, parse, render_codespan, Severity};
use std::env;
use std::fs;
use std::io::{self, Read};
use std::process::ExitCode;

fn main() -> ExitCode {
    let mut args = env::args().skip(1);
    let cmd = args.next().unwrap_or_else(|| "check".into());
    if cmd == "-h" || cmd == "--help" {
        eprintln!("usage: clpp-front <tokens|tree|ast|check> <file|->");
        return ExitCode::SUCCESS;
    }
    let path = args.next().unwrap_or_else(|| "-".into());
    let source = read_source(&path).unwrap_or_else(|err| {
        eprintln!("{err}");
        std::process::exit(1);
    });
    let name = if path == "-" { "<stdin>" } else { &path };
    match cmd.as_str() {
        "tokens" | "lex" => {
            let lexed = lex(&source);
            for tok in &lexed.tokens {
                let text = &source[tok.span.start as usize..tok.span.end as usize];
                println!(
                    "{:?}\t{}..{}\t{}",
                    tok.kind,
                    tok.span.start,
                    tok.span.end,
                    text.replace('\n', "\\n")
                );
            }
            emit_diags(name, &source, &lexed.diagnostics);
            status(&lexed.diagnostics)
        }
        "tree" | "cst" => {
            let parsed = parse(name, &source);
            print!("{}", dump_cst(&parsed.syntax));
            emit_diags(name, &source, &parsed.diagnostics);
            status(&parsed.diagnostics)
        }
        "ast" => {
            let parsed = parse(name, &source);
            print!("{}", dump_ast(&parsed.ast));
            emit_diags(name, &source, &parsed.diagnostics);
            status(&parsed.diagnostics)
        }
        "check" | "parse" => {
            let parsed = parse(name, &source);
            emit_diags(name, &source, &parsed.diagnostics);
            if parsed.ok() {
                println!("ok {}", parsed.ast.items.len());
            }
            status(&parsed.diagnostics)
        }
        other => {
            eprintln!("unknown command {other}");
            eprintln!("usage: clpp-front <tokens|tree|ast|check> <file|->");
            ExitCode::from(2)
        }
    }
}

fn read_source(path: &str) -> io::Result<String> {
    if path == "-" {
        let mut buf = String::new();
        io::stdin().read_to_string(&mut buf)?;
        Ok(buf)
    } else {
        fs::read_to_string(path)
    }
}

fn emit_diags(name: &str, source: &str, diags: &[clpp_front::Diagnostic]) {
    if diags.is_empty() {
        return;
    }
    eprint!("{}", render_codespan(name, source, diags));
}

fn status(diags: &[clpp_front::Diagnostic]) -> ExitCode {
    if diags.iter().any(|d| d.severity == Severity::Error) {
        ExitCode::from(1)
    } else {
        ExitCode::SUCCESS
    }
}
