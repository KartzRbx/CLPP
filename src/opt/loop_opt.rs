//! Loop opts: hoist loop-invariant const decls (RFC 0011 §6 lite).

use crate::ast::{Expr, Item, Program, Stmt};
use std::collections::HashSet;

pub fn run(program: &mut Program) {
    for item in &mut program.items {
        if let Item::Function(func) | Item::Proto(func) = item {
            func.body = hoist_in_stmts(std::mem::take(&mut func.body));
        }
    }
}

fn hoist_in_stmts(stmts: Vec<Stmt>) -> Vec<Stmt> {
    stmts.into_iter().map(hoist_stmt).collect()
}

fn hoist_stmt(stmt: Stmt) -> Stmt {
    match stmt {
        Stmt::While { test, body } => {
            let (hoisted, body) = split_invariant(body, &HashSet::new());
            let mut out = hoisted;
            out.push(Stmt::While {
                test,
                body: hoist_in_stmts(body),
            });
            if out.len() == 1 {
                out.pop().unwrap()
            } else {
                Stmt::Block(out)
            }
        }
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => {
            let mut carried = HashSet::new();
            if let Some(Stmt::Decl(d)) = init.as_deref() {
                carried.insert(d.name.clone());
            }
            // Also treat induction updates as carried via incr target.
            if let Some(incr_expr) = &incr {
                collect_assigned(incr_expr, &mut carried);
            }
            let (hoisted, body) = split_invariant(body, &carried);
            let mut prefix = hoisted;
            prefix.push(Stmt::CFor {
                init,
                test,
                incr,
                body: hoist_in_stmts(body),
            });
            if prefix.len() == 1 {
                prefix.pop().unwrap()
            } else {
                Stmt::Block(prefix)
            }
        }
        Stmt::ForEach {
            name,
            elem_type,
            iter,
            body,
            span,
        } => {
            let mut carried = HashSet::new();
            carried.insert(name.clone());
            let (hoisted, body) = split_invariant(body, &carried);
            let mut prefix = hoisted;
            prefix.push(Stmt::ForEach {
                name,
                elem_type,
                iter,
                body: hoist_in_stmts(body),
                span,
            });
            if prefix.len() == 1 {
                prefix.pop().unwrap()
            } else {
                Stmt::Block(prefix)
            }
        }
        Stmt::Block(body) => Stmt::Block(hoist_in_stmts(body)),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => Stmt::If {
            test,
            consequent: hoist_in_stmts(consequent),
            alternate: alternate.map(hoist_in_stmts),
        },
        other => other,
    }
}

fn split_invariant(body: Vec<Stmt>, carried: &HashSet<String>) -> (Vec<Stmt>, Vec<Stmt>) {
    let mut hoisted = Vec::new();
    let mut rest = Vec::new();
    let mut blocked = carried.clone();
    for stmt in body {
        match &stmt {
            Stmt::Decl(decl)
                if decl.is_const
                    && decl
                        .value
                        .as_ref()
                        .is_some_and(|v| expr_invariant(v, &blocked)) =>
            {
                hoisted.push(stmt);
            }
            other => {
                // Once we see a non-hoistable stmt, later decls may depend on it.
                collect_stmt_defs(other, &mut blocked);
                rest.push(stmt);
            }
        }
    }
    (hoisted, rest)
}

fn expr_invariant(expr: &Expr, blocked: &HashSet<String>) -> bool {
    match expr {
        Expr::Null | Expr::Bool(_) | Expr::Number(_) | Expr::String(_) => true,
        Expr::Ident(n) => !blocked.contains(n),
        Expr::Unary { argument, .. } | Expr::Cast { argument, .. } | Expr::Try { argument } => {
            expr_invariant(argument, blocked)
        }
        Expr::Binary { left, right, .. } | Expr::Coalesce { left, right } => {
            // Strength reduction fodder: i*2 stays; pure arithmetic of invariants is invariant.
            expr_invariant(left, blocked) && expr_invariant(right, blocked)
        }
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => {
            expr_invariant(cond, blocked)
                && expr_invariant(then_expr, blocked)
                && expr_invariant(else_expr, blocked)
        }
        Expr::Member { object, .. } => expr_invariant(object, blocked),
        // Calls / news / assigns are not hoisted (may have side effects).
        _ => false,
    }
}

fn collect_assigned(expr: &Expr, out: &mut HashSet<String>) {
    match expr {
        Expr::Assign { left, .. } | Expr::Update { target: left, .. } => {
            if let Expr::Ident(n) = left.as_ref() {
                out.insert(n.clone());
            }
        }
        Expr::Binary { left, right, .. } => {
            collect_assigned(left, out);
            collect_assigned(right, out);
        }
        _ => {}
    }
}

fn collect_stmt_defs(stmt: &Stmt, out: &mut HashSet<String>) {
    match stmt {
        Stmt::Decl(d) => {
            out.insert(d.name.clone());
        }
        Stmt::Expr(e) => collect_assigned(e, out),
        _ => {}
    }
}
