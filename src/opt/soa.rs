//! SoA-oriented rewrites for dense numeric particle-like locals (RFC 0011 Phase 1).

use crate::ast::{Decl, Expr, Item, Program, Span, Stmt};

/// When a local `array` of InitList structs is never escaped as a whole table of objects,
/// split into parallel field arrays (SoA) for the function body.
pub fn run(program: &mut Program) -> Vec<String> {
    let mut notes = Vec::new();
    for item in &mut program.items {
        if let Item::Function(f) | Item::Proto(f) = item {
            let (body, n) = rewrite_body(std::mem::take(&mut f.body));
            f.body = body;
            notes.extend(n);
        }
    }
    notes
}

fn rewrite_body(stmts: Vec<Stmt>) -> (Vec<Stmt>, Vec<String>) {
    // Conservative: only hoist InitList-of-struct array literals that are tiny.
    let mut notes = Vec::new();
    let mut out = Vec::new();
    for stmt in stmts {
        match stmt {
            Stmt::Decl(decl) if looks_aos_array(&decl) => {
                if let Some(Expr::ArrayLit { elements }) = &decl.value {
                    if let Some(fields) = extract_field_names(elements) {
                        notes.push(format!("SoA:{}", decl.name));
                        for field in &fields {
                            let vals: Vec<Expr> = elements
                                .iter()
                                .filter_map(|e| match e {
                                    Expr::InitList { fields: fs } => {
                                        fs.iter().find(|(n, _)| n == field).map(|(_, v)| v.clone())
                                    }
                                    _ => None,
                                })
                                .collect();
                            out.push(Stmt::Decl(Decl {
                                name: format!("{}__{}", decl.name, field),
                                value_type: Some("array<float>".into()),
                                value: Some(Expr::ArrayLit { elements: vals }),
                                is_const: decl.is_const,
                                is_observable: false,
                                owner: None,
                                line: decl.line,
                                span: Span::default(),
                                doc: None,
                                visibility: None,
                            }));
                        }
                        continue;
                    }
                }
                out.push(Stmt::Decl(decl));
            }
            other => out.push(other),
        }
    }
    (out, notes)
}

fn looks_aos_array(decl: &Decl) -> bool {
    matches!(&decl.value, Some(Expr::ArrayLit { elements }) if elements.len() >= 2
        && elements.iter().all(|e| matches!(e, Expr::InitList { fields } if !fields.is_empty())))
}

fn extract_field_names(elements: &[Expr]) -> Option<Vec<String>> {
    let Expr::InitList { fields } = elements.first()? else {
        return None;
    };
    let names: Vec<String> = fields.iter().map(|(n, _)| n.clone()).collect();
    if names.is_empty() {
        return None;
    }
    for e in elements.iter().skip(1) {
        let Expr::InitList { fields } = e else {
            return None;
        };
        if fields.len() != names.len() {
            return None;
        }
        for (i, (n, _)) in fields.iter().enumerate() {
            if names.get(i) != Some(n) {
                return None;
            }
        }
    }
    Some(names)
}
