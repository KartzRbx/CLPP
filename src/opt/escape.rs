//! Escape analysis beyond Vector InitList (RFC 0011 Phase 1).

use crate::ast::{Expr, Stmt};
use crate::names::is_library_type;

/// Datatypes whose `.new(...)` + field reads can become scalars when they do not escape.
pub fn vector_fields(class_name: &str) -> Option<&'static [&'static str]> {
    match class_name {
        "Vector2" | "Vector2int16" => Some(&["X", "Y"]),
        "Vector3" | "Vector3int16" => Some(&["X", "Y", "Z"]),
        "CFrame" => Some(&["X", "Y", "Z"]),
        "Color3" => Some(&["R", "G", "B"]),
        _ => None,
    }
}

pub fn is_scalarizable_new(class_name: &str) -> bool {
    vector_fields(class_name).is_some() && is_library_type(class_name)
}

/// Map positional `Type.new(a,b,…)` args onto field names.
pub fn zip_vector_args(class_name: &str, args: &[Expr]) -> Option<Vec<(String, Expr)>> {
    let fields = vector_fields(class_name)?;
    if args.len() < fields.len() {
        return None;
    }
    Some(
        fields
            .iter()
            .zip(args.iter())
            .map(|(f, a)| ((*f).to_string(), a.clone()))
            .collect(),
    )
}

/// True if `name` escapes the local scope as a whole value (passed to call, returned, stored in table, assigned to field).
#[allow(dead_code)]
pub fn escapes(name: &str, stmts: &[Stmt]) -> bool {
    stmts.iter().any(|s| stmt_escapes(name, s))
}

#[allow(dead_code)]
fn stmt_escapes(name: &str, stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Return(Some(e)) => expr_escapes(name, e, true),
        Stmt::Expr(e) => expr_escapes(name, e, false),
        Stmt::Decl(d) => d
            .value
            .as_ref()
            .is_some_and(|v| expr_escapes(name, v, false)),
        Stmt::Block(b)
        | Stmt::While { body: b, .. }
        | Stmt::ForEach { body: b, .. }
        | Stmt::CFor { body: b, .. } => escapes(name, b),
        Stmt::If {
            consequent,
            alternate,
            test,
            ..
        } => {
            expr_escapes(name, test, false)
                || escapes(name, consequent)
                || alternate.as_ref().is_some_and(|a| escapes(name, a))
        }
        Stmt::Match {
            discriminant,
            arms,
        } => {
            expr_escapes(name, discriminant, false)
                || arms.iter().any(|a| escapes(name, &a.body))
        }
        _ => false,
    }
}

fn expr_escapes(name: &str, expr: &Expr, returning: bool) -> bool {
    match expr {
        Expr::Ident(n) => returning && n == name,
        Expr::Call { object, args, .. } => {
            object.as_ref().is_some_and(|o| whole_ident(name, o))
                || args.iter().any(|a| whole_ident(name, a))
        }
        Expr::Assign { left, right, .. } => {
            whole_ident(name, right)
                || matches!(left.as_ref(), Expr::Member { .. } if whole_ident(name, right))
        }
        Expr::Member { object, .. } => {
            // Field read of name is not an escape of the whole value.
            !matches!(object.as_ref(), Expr::Ident(n) if n == name)
                && expr_escapes(name, object, returning)
        }
        Expr::Unary { argument, .. }
        | Expr::Await { argument }
        | Expr::Cast { argument, .. }
        | Expr::Try { argument }
        | Expr::Update { target: argument, .. } => expr_escapes(name, argument, returning),
        Expr::Binary { left, right, .. }
        | Expr::Coalesce { left, right }
        | Expr::Index {
            object: left,
            index: right,
        } => expr_escapes(name, left, returning) || expr_escapes(name, right, returning),
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => {
            expr_escapes(name, cond, returning)
                || expr_escapes(name, then_expr, returning)
                || expr_escapes(name, else_expr, returning)
        }
        Expr::New { args, .. } | Expr::Tuple(args) | Expr::ArrayLit { elements: args } => {
            args.iter().any(|a| whole_ident(name, a))
        }
        Expr::InitList { fields } => fields.iter().any(|(_, v)| whole_ident(name, v)),
        _ => false,
    }
}

fn whole_ident(name: &str, expr: &Expr) -> bool {
    matches!(expr, Expr::Ident(n) if n == name)
}
