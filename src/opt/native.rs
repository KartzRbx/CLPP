//! Selective `@native` *hints* for Cluaupp (RFC 0011 §8).
//!
//! Does **not** auto-emit `@native` (Roblox warns against blanket native).
//! Collects function names that look numeric-hot / API-light for the host.

use crate::ast::{Expr, Function, Item, Program, Stmt};

pub fn run(program: &Program) -> Vec<String> {
    let mut hints = Vec::new();
    for item in &program.items {
        if let Item::Function(func) = item {
            if should_hint(func) {
                hints.push(qualified(func));
            }
        }
    }
    hints.sort();
    hints.dedup();
    hints
}

fn qualified(func: &Function) -> String {
    match &func.owner {
        Some(o) => format!("{o}.{}", func.name),
        None => func.name.clone(),
    }
}

fn should_hint(func: &Function) -> bool {
    if func.attrs.iter().any(|a| a.name == "native" || a.name == "nonative") {
        return false;
    }
    if func.body.is_empty() {
        return false;
    }
    let mut score = 0i32;
    let mut api = 0i32;
    score_stmts(&func.body, &mut score, &mut api);
    // Hot numeric + low Roblox API surface.
    score >= 4 && api <= 1
}

fn score_stmts(stmts: &[Stmt], score: &mut i32, api: &mut i32) {
    for stmt in stmts {
        match stmt {
            Stmt::While { body, .. }
            | Stmt::DoWhile { body, .. }
            | Stmt::ForEach { body, .. }
            | Stmt::CFor { body, .. } => {
                *score += 3;
                score_stmts(body, score, api);
            }
            Stmt::If {
                consequent,
                alternate,
                test,
                ..
            } => {
                score_expr(test, score, api);
                score_stmts(consequent, score, api);
                if let Some(a) = alternate {
                    score_stmts(a, score, api);
                }
            }
            Stmt::Expr(e) | Stmt::Return(Some(e)) => score_expr(e, score, api),
            Stmt::Decl(d) => {
                if let Some(v) = &d.value {
                    score_expr(v, score, api);
                }
            }
            Stmt::Block(body) | Stmt::Spawn { body, .. } => score_stmts(body, score, api),
            _ => {}
        }
    }
}

fn score_expr(expr: &Expr, score: &mut i32, api: &mut i32) {
    match expr {
        Expr::Number(_) => *score += 1,
        Expr::Binary {
            op,
            left,
            right,
        } => {
            if matches!(op.as_str(), "+" | "-" | "*" | "/" | "%" | "**") {
                *score += 1;
            }
            score_expr(left, score, api);
            score_expr(right, score, api);
        }
        Expr::Call {
            object,
            name,
            args,
            ..
        } => {
            if is_robloxish(name) {
                *api += 2;
            }
            if let Some(o) = object {
                score_expr(o, score, api);
            }
            for a in args {
                score_expr(a, score, api);
            }
        }
        Expr::New { class_name, args } => {
            if is_instance_like(class_name) {
                *api += 2;
            }
            for a in args {
                score_expr(a, score, api);
            }
        }
        Expr::Member { object, name, .. } => {
            if is_robloxish(name) {
                *api += 1;
            }
            score_expr(object, score, api);
        }
        Expr::Assign {
            left,
            right,
            ..
        } => {
            score_expr(left, score, api);
            score_expr(right, score, api);
        }
        Expr::Update { target, .. } => score_expr(target, score, api),
        Expr::Unary { argument, .. } | Expr::Cast { argument, .. } => {
            score_expr(argument, score, api);
        }
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => {
            score_expr(cond, score, api);
            score_expr(then_expr, score, api);
            score_expr(else_expr, score, api);
        }
        Expr::Index { object, index } => {
            score_expr(object, score, api);
            score_expr(index, score, api);
        }
        _ => {}
    }
}

fn is_robloxish(name: &str) -> bool {
    matches!(
        name,
        "GetService"
            | "WaitForChild"
            | "FindFirstChild"
            | "Clone"
            | "Destroy"
            | "Connect"
            | "Once"
            | "Fire"
            | "FireServer"
            | "FireClient"
            | "Invoke"
            | "InvokeServer"
            | "post"
            | "print"
            | "warn"
    )
}

fn is_instance_like(name: &str) -> bool {
    matches!(
        name,
        "Instance"
            | "Part"
            | "Folder"
            | "RemoteEvent"
            | "RemoteFunction"
            | "ScreenGui"
            | "Frame"
            | "TextLabel"
            | "IntValue"
            | "StringValue"
            | "BoolValue"
            | "NumberValue"
    )
}
