//! Phase-1 CL++ front end.
//!
//! Each stage is a pure function of its inputs:
//! `lex` → tokens, `parse_tokens` → lossless CST, `lower` → typed AST.
//! Nothing here knows about a host runtime. A later incremental layer can
//! memoize these functions without changing their signatures.
//!
//! `//` is a line comment unless the next character is `=`, in which case the
//! token is `//=` (floor-division assignment). See `lex` for the rule.

pub mod ast;
pub mod diag;
pub mod kind;
pub mod lex;
pub mod link;
pub mod parse;
pub mod span;

pub use ast::{dump_ast, dump_cst, lower, Item, Program, Ty, TyKind};
pub use diag::{render, render_codespan, ApiDiagnostic, Diagnostic, Severity};
pub use kind::{SyntaxKind, SyntaxNode};
pub use lex::{lex, Lexed, Token};
pub use link::{file_stem, link_binding_name, LinkTarget};
pub use span::{LineIndex, Span};

#[derive(Clone, Debug)]
pub struct Parsed {
    pub file_name: String,
    pub syntax: SyntaxNode,
    pub ast: Program,
    pub diagnostics: Vec<Diagnostic>,
}

impl Parsed {
    pub fn ok(&self) -> bool {
        !self
            .diagnostics
            .iter()
            .any(|d| d.severity == Severity::Error)
    }
}

/// Lex, parse, and lower one file. Always returns a tree.
pub fn parse(file_name: &str, source: &str) -> Parsed {
    let lexed = lex(source);
    let (syntax, parse_diags) = parse::parse_tokens(source, &lexed.tokens);
    let ast = lower(&syntax);
    let mut diagnostics = lexed.diagnostics;
    diagnostics.extend(parse_diags);
    Parsed {
        file_name: file_name.to_string(),
        syntax,
        ast,
        diagnostics,
    }
}
