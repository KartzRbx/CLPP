//! Dense layout / buffer / SoA candidate hints (RFC 0011 Phase 1 lite).

use crate::ast::{Decl, Expr, Item, Program, Stmt};

/// Hint string for Cluaupp / profilers (`DenseNumeric:name`, `SoACandidate:Type`).
pub fn run(program: &Program) -> Vec<String> {
    let mut hints = Vec::new();
    for item in &program.items {
        match item {
            Item::Decl(decl) => consider_decl(decl, &mut hints),
            Item::Function(f) | Item::Proto(f) => {
                for p in &f.params {
                    if let Some(ty) = &p.value_type {
                        if is_dense_numeric_array(ty) {
                            hints.push(format!("DenseNumeric:{}", p.name));
                            hints.push(format!("BufferCandidate:{}", p.name));
                        }
                    }
                }
                walk_stmts(&f.body, &mut hints);
            }
            Item::Class { name, .. } if looks_soa_struct(name) => {
                hints.push(format!("SoACandidate:{name}"));
            }
            _ => {}
        }
    }
    hints.sort();
    hints.dedup();
    hints
}

fn walk_stmts(stmts: &[Stmt], hints: &mut Vec<String>) {
    for stmt in stmts {
        match stmt {
            Stmt::Decl(d) => consider_decl(d, hints),
            Stmt::Block(b)
            | Stmt::While { body: b, .. }
            | Stmt::DoWhile { body: b, .. }
            | Stmt::ForEach { body: b, .. }
            | Stmt::Spawn { body: b, .. }
            | Stmt::Defer { body: b }
            | Stmt::Comptime { body: b, .. } => walk_stmts(b, hints),
            Stmt::CFor { body, init, .. } => {
                if let Some(s) = init {
                    if let Stmt::Decl(d) = s.as_ref() {
                        consider_decl(d, hints);
                    }
                }
                walk_stmts(body, hints);
            }
            Stmt::If {
                consequent,
                alternate,
                ..
            } => {
                walk_stmts(consequent, hints);
                if let Some(a) = alternate {
                    walk_stmts(a, hints);
                }
            }
            _ => {}
        }
    }
}

fn consider_decl(decl: &Decl, hints: &mut Vec<String>) {
    let ty = decl.value_type.as_deref().unwrap_or("");
    if is_dense_numeric_array(ty) {
        hints.push(format!("DenseNumeric:{}", decl.name));
        hints.push(format!("BufferCandidate:{}", decl.name));
        return;
    }
    if let Some(Expr::ArrayLit { elements }) = &decl.value {
        if elements.len() >= 8 && elements.iter().all(|e| matches!(e, Expr::Number(_))) {
            hints.push(format!("DenseNumeric:{}", decl.name));
            hints.push(format!("BufferCandidate:{}", decl.name));
        }
    }
}

fn is_dense_numeric_array(ty: &str) -> bool {
    let t = ty.replace(' ', "");
    t.starts_with("array<float")
        || t.starts_with("array<int")
        || t.starts_with("array<number")
        || t.starts_with("vector<float")
        || t.starts_with("vector<int")
        || t == "array<float>"
        || t == "array<int>"
}

fn looks_soa_struct(name: &str) -> bool {
    matches!(
        name,
        "Particle" | "Particles" | "Transform" | "RigidBody" | "Boid" | "Projectile"
    )
}
