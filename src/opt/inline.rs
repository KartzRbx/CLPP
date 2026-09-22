//! Selective inlining of small, pure, free functions (RFC 0011 §4).

use crate::ast::{Expr, Function, Item, Program, Stmt};
use std::collections::{HashMap, HashSet};

const MAX_BODY_STMTS: usize = 3;

pub fn run(program: &mut Program) {
    let candidates = collect_candidates(program);
    if candidates.is_empty() {
        return;
    }
    for item in &mut program.items {
        if let Item::Function(func) | Item::Proto(func) = item {
            // Do not rewrite a function while inlining itself into callers of peers.
            let skip = candidates.contains_key(&func.name);
            if skip && func.owner.is_none() {
                // Still allow inlining *other* candidates into this body.
            }
            let mut env = candidates.clone();
            // Never inline a function into itself.
            env.remove(&func.name);
            func.body = rewrite_stmts(std::mem::take(&mut func.body), &env);
        }
    }
}

fn collect_candidates(program: &Program) -> HashMap<String, Function> {
    let mut out = HashMap::new();
    for item in &program.items {
        if let Item::Function(func) = item {
            if func.owner.is_none() && is_candidate(func) {
                out.insert(func.name.clone(), func.clone());
            }
        }
    }
    out
}

fn is_candidate(func: &Function) -> bool {
    if func.is_async || !func.type_params.is_empty() {
        return false;
    }
    if func.attrs.iter().any(|a| a.name == "noinline" || a.name == "native") {
        return false;
    }
    if func.body.is_empty() || func.body.len() > MAX_BODY_STMTS {
        return false;
    }
    let params: HashSet<String> = func.params.iter().map(|p| p.name.clone()).collect();
    let mut saw_return = false;
    for stmt in &func.body {
        match stmt {
            Stmt::Return(Some(expr)) => {
                if !expr_is_pure(expr, &params) {
                    return false;
                }
                saw_return = true;
            }
            Stmt::Return(None) => saw_return = true,
            Stmt::Decl(decl) if decl.is_const => {
                if let Some(v) = &decl.value {
                    if !expr_is_pure(v, &params) {
                        return false;
                    }
                }
            }
            _ => return false,
        }
    }
    saw_return
}

fn expr_is_pure(expr: &Expr, params: &HashSet<String>) -> bool {
    match expr {
        Expr::Null | Expr::Bool(_) | Expr::Number(_) | Expr::String(_) => true,
        Expr::Ident(name) => params.contains(name),
        Expr::Unary { argument, .. } => expr_is_pure(argument, params),
        Expr::Binary { left, right, .. } => {
            expr_is_pure(left, params) && expr_is_pure(right, params)
        }
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => {
            expr_is_pure(cond, params)
                && expr_is_pure(then_expr, params)
                && expr_is_pure(else_expr, params)
        }
        Expr::Coalesce { left, right } => {
            expr_is_pure(left, params) && expr_is_pure(right, params)
        }
        Expr::Cast { argument, .. } => expr_is_pure(argument, params),
        Expr::Tuple(values) => values.iter().all(|v| expr_is_pure(v, params)),
        _ => false,
    }
}

fn rewrite_stmts(stmts: Vec<Stmt>, env: &HashMap<String, Function>) -> Vec<Stmt> {
    stmts
        .into_iter()
        .map(|s| rewrite_stmt(s, env))
        .collect()
}

fn rewrite_stmt(stmt: Stmt, env: &HashMap<String, Function>) -> Stmt {
    match stmt {
        Stmt::Decl(mut decl) => {
            if let Some(v) = decl.value.take() {
                decl.value = Some(rewrite_expr(v, env));
            }
            Stmt::Decl(decl)
        }
        Stmt::Expr(expr) => Stmt::Expr(rewrite_expr(expr, env)),
        Stmt::Return(Some(expr)) => Stmt::Return(Some(rewrite_expr(expr, env))),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => Stmt::If {
            test: rewrite_expr(test, env),
            consequent: rewrite_stmts(consequent, env),
            alternate: alternate.map(|a| rewrite_stmts(a, env)),
        },
        Stmt::While { test, body } => Stmt::While {
            test: rewrite_expr(test, env),
            body: rewrite_stmts(body, env),
        },
        Stmt::Block(body) => Stmt::Block(rewrite_stmts(body, env)),
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => Stmt::CFor {
            init: init.map(|s| Box::new(rewrite_stmt(*s, env))),
            test: test.map(|t| rewrite_expr(t, env)),
            incr: incr.map(|t| rewrite_expr(t, env)),
            body: rewrite_stmts(body, env),
        },
        Stmt::ForEach {
            name,
            elem_type,
            iter,
            body,
            span,
        } => Stmt::ForEach {
            name,
            elem_type,
            iter: rewrite_expr(iter, env),
            body: rewrite_stmts(body, env),
            span,
        },
        Stmt::DoWhile { body, test } => Stmt::DoWhile {
            body: rewrite_stmts(body, env),
            test: rewrite_expr(test, env),
        },
        Stmt::Guard { test, body } => Stmt::Guard {
            test: rewrite_expr(test, env),
            body: rewrite_stmts(body, env),
        },
        other => other,
    }
}

fn rewrite_expr(expr: Expr, env: &HashMap<String, Function>) -> Expr {
    match expr {
        Expr::Call {
            object: None,
            name,
            args,
            access,
            type_args,
        } if type_args.is_empty() => {
            let args: Vec<Expr> = args.into_iter().map(|a| rewrite_expr(a, env)).collect();
            if let Some(callee) = env.get(&name) {
                if let Some(inlined) = try_inline(callee, &args) {
                    return inlined;
                }
            }
            Expr::Call {
                object: None,
                name,
                args,
                access,
                type_args,
            }
        }
        Expr::Call {
            object,
            name,
            args,
            access,
            type_args,
        } => Expr::Call {
            object: object.map(|o| Box::new(rewrite_expr(*o, env))),
            name,
            args: args.into_iter().map(|a| rewrite_expr(a, env)).collect(),
            access,
            type_args,
        },
        Expr::Unary { op, argument } => Expr::Unary {
            op,
            argument: Box::new(rewrite_expr(*argument, env)),
        },
        Expr::Binary { op, left, right } => Expr::Binary {
            op,
            left: Box::new(rewrite_expr(*left, env)),
            right: Box::new(rewrite_expr(*right, env)),
        },
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => Expr::Ternary {
            cond: Box::new(rewrite_expr(*cond, env)),
            then_expr: Box::new(rewrite_expr(*then_expr, env)),
            else_expr: Box::new(rewrite_expr(*else_expr, env)),
        },
        Expr::Member {
            object,
            name,
            access,
        } => Expr::Member {
            object: Box::new(rewrite_expr(*object, env)),
            name,
            access,
        },
        Expr::Assign {
            op,
            left,
            right,
            line,
        } => Expr::Assign {
            op,
            left: Box::new(rewrite_expr(*left, env)),
            right: Box::new(rewrite_expr(*right, env)),
            line,
        },
        Expr::Cast {
            value_type,
            argument,
            kind,
        } => Expr::Cast {
            value_type,
            argument: Box::new(rewrite_expr(*argument, env)),
            kind,
        },
        Expr::Index { object, index } => Expr::Index {
            object: Box::new(rewrite_expr(*object, env)),
            index: Box::new(rewrite_expr(*index, env)),
        },
        Expr::Coalesce { left, right } => Expr::Coalesce {
            left: Box::new(rewrite_expr(*left, env)),
            right: Box::new(rewrite_expr(*right, env)),
        },
        other => other,
    }
}

fn try_inline(callee: &Function, args: &[Expr]) -> Option<Expr> {
    if args.len() != callee.params.len() {
        return None;
    }
    let mut map: HashMap<String, Expr> = HashMap::new();
    for (p, a) in callee.params.iter().zip(args.iter()) {
        map.insert(p.name.clone(), a.clone());
    }
    // Bind local consts inside callee first.
    for stmt in &callee.body {
        if let Stmt::Decl(decl) = stmt {
            if decl.is_const {
                if let Some(v) = &decl.value {
                    map.insert(decl.name.clone(), subst_expr(v, &map));
                }
            }
        }
    }
    for stmt in &callee.body {
        if let Stmt::Return(Some(expr)) = stmt {
            return Some(subst_expr(expr, &map));
        }
    }
    None
}

fn subst_expr(expr: &Expr, map: &HashMap<String, Expr>) -> Expr {
    match expr {
        Expr::Ident(name) => map
            .get(name)
            .cloned()
            .unwrap_or_else(|| Expr::Ident(name.clone())),
        Expr::Unary { op, argument } => Expr::Unary {
            op: op.clone(),
            argument: Box::new(subst_expr(argument, map)),
        },
        Expr::Binary { op, left, right } => Expr::Binary {
            op: op.clone(),
            left: Box::new(subst_expr(left, map)),
            right: Box::new(subst_expr(right, map)),
        },
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => Expr::Ternary {
            cond: Box::new(subst_expr(cond, map)),
            then_expr: Box::new(subst_expr(then_expr, map)),
            else_expr: Box::new(subst_expr(else_expr, map)),
        },
        Expr::Coalesce { left, right } => Expr::Coalesce {
            left: Box::new(subst_expr(left, map)),
            right: Box::new(subst_expr(right, map)),
        },
        Expr::Cast {
            value_type,
            argument,
            kind,
        } => Expr::Cast {
            value_type: value_type.clone(),
            argument: Box::new(subst_expr(argument, map)),
            kind: kind.clone(),
        },
        Expr::Tuple(values) => Expr::Tuple(values.iter().map(|v| subst_expr(v, map)).collect()),
        other => other.clone(),
    }
}
