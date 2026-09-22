//! Buffer specialization comments / metadata for dense numeric arrays (RFC 0011).

use crate::ast::{Expr, Item, Program, Stmt};

/// Annotate program decls that are buffer candidates by rewriting ArrayLit of numbers
/// into a comment-friendly shape: keep ArrayLit but record names for emitter.
pub fn run(program: &Program) -> Vec<String> {
    let mut out = Vec::new();
    for item in &program.items {
        match item {
            Item::Function(f) | Item::Proto(f) => walk(&f.body, &mut out),
            Item::Decl(d) => consider(d.name.as_str(), d.value.as_ref(), &mut out),
            _ => {}
        }
    }
    out.sort();
    out.dedup();
    out
}

fn walk(stmts: &[Stmt], out: &mut Vec<String>) {
    for s in stmts {
        match s {
            Stmt::Decl(d) => consider(d.name.as_str(), d.value.as_ref(), out),
            Stmt::Block(b)
            | Stmt::While { body: b, .. }
            | Stmt::ForEach { body: b, .. }
            | Stmt::CFor { body: b, .. } => walk(b, out),
            Stmt::If {
                consequent,
                alternate,
                ..
            } => {
                walk(consequent, out);
                if let Some(a) = alternate {
                    walk(a, out);
                }
            }
            _ => {}
        }
    }
}

fn consider(name: &str, value: Option<&Expr>, out: &mut Vec<String>) {
    if let Some(Expr::ArrayLit { elements }) = value {
        if elements.len() >= 8 && elements.iter().all(|e| matches!(e, Expr::Number(_))) {
            out.push(format!("BufferSpecialize:{name}"));
        }
    }
}
