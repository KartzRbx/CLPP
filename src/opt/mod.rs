//! High-level AST optimizations (RFC 0011).
//!
//! Fold / prop / DCE → inline → scalar/escape → loop hoist → mono → layout/native hints.
//! Not a Luau peephole pass — leave low-level opts to Luau / native.

mod escape;
mod fold;
mod inline;
mod layout;
mod loop_opt;
mod mono;
mod native;
mod scalar;

use crate::ast::{Function, Item, Program, Stmt};
use fold::{eval_bool, fold_expr, ConstEnv};

/// Summary of opts useful to Cluaupp / tooling (never a second JIT).
#[derive(Debug, Clone, Default)]
pub struct OptReport {
    /// Functions that look like good selective `@native` candidates.
    pub native_hints: Vec<String>,
    /// Specialized generic function names emitted this compile.
    pub specialized: Vec<String>,
    /// Dense / SoA / buffer layout candidates.
    pub layout_hints: Vec<String>,
}

/// Run high-level opts in place. Safe for any well-typed program.
pub fn optimize(program: &mut Program) -> OptReport {
    fold_and_dce(program);
    inline::run(program);
    fold_and_dce(program);
    scalar::run(program);
    loop_opt::run(program);
    let specialized = mono::run(program);
    fold_and_dce(program);
    let native_hints = native::run(program);
    let layout_hints = layout::run(program);
    OptReport {
        native_hints,
        specialized,
        layout_hints,
    }
}

fn fold_and_dce(program: &mut Program) {
    let mut file_env = ConstEnv::default();
    for item in &program.items {
        if let Item::Decl(decl) = item {
            if decl.is_const {
                if let Some(v) = &decl.value {
                    file_env.bind(&decl.name, v);
                }
            }
        }
    }
    for item in &mut program.items {
        match item {
            Item::Function(func) | Item::Proto(func) => {
                optimize_function(func, &file_env);
            }
            Item::Decl(decl) => {
                if let Some(v) = &mut decl.value {
                    *v = fold_expr(v, &file_env);
                }
            }
            _ => {}
        }
    }
}

fn optimize_function(func: &mut Function, file_env: &ConstEnv) {
    let mut env = file_env.clone();
    func.body = optimize_stmts(std::mem::take(&mut func.body), &mut env);
}

fn optimize_stmts(stmts: Vec<Stmt>, env: &mut ConstEnv) -> Vec<Stmt> {
    let mut out = Vec::with_capacity(stmts.len());
    for stmt in stmts {
        match optimize_stmt(stmt, env) {
            Some(Stmt::Block(inner)) if inner.is_empty() => {}
            Some(Stmt::Block(inner)) => out.extend(inner),
            Some(s) => out.push(s),
            None => {}
        }
    }
    out
}

fn optimize_stmt(stmt: Stmt, env: &mut ConstEnv) -> Option<Stmt> {
    match stmt {
        Stmt::Decl(mut decl) => {
            if let Some(v) = &mut decl.value {
                *v = fold_expr(v, env);
            }
            if decl.is_const {
                if let Some(v) = &decl.value {
                    env.bind(&decl.name, v);
                }
            }
            Some(Stmt::Decl(decl))
        }
        Stmt::Expr(expr) => Some(Stmt::Expr(fold_expr(&expr, env))),
        Stmt::Return(Some(expr)) => Some(Stmt::Return(Some(fold_expr(&expr, env)))),
        Stmt::Return(None) => Some(Stmt::Return(None)),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => {
            let test = fold_expr(&test, env);
            match eval_bool(&test, env) {
                Some(true) => {
                    let body = optimize_stmts(consequent, env);
                    Some(Stmt::Block(body))
                }
                Some(false) => {
                    if let Some(alt) = alternate {
                        let body = optimize_stmts(alt, env);
                        Some(Stmt::Block(body))
                    } else {
                        None
                    }
                }
                None => {
                    let consequent = optimize_stmts(consequent, &mut env.clone());
                    let alternate = alternate.map(|a| optimize_stmts(a, &mut env.clone()));
                    Some(Stmt::If {
                        test,
                        consequent,
                        alternate,
                    })
                }
            }
        }
        Stmt::Guard { test, body } => {
            let test = fold_expr(&test, env);
            match eval_bool(&test, env) {
                Some(true) => None,
                Some(false) => {
                    let body = optimize_stmts(body, env);
                    Some(Stmt::Block(body))
                }
                None => {
                    let body = optimize_stmts(body, &mut env.clone());
                    Some(Stmt::Guard { test, body })
                }
            }
        }
        Stmt::While { test, body } => {
            let test = fold_expr(&test, env);
            if eval_bool(&test, env) == Some(false) {
                return None;
            }
            let body = optimize_stmts(body, &mut env.clone());
            Some(Stmt::While { test, body })
        }
        Stmt::Block(body) => {
            let body = optimize_stmts(body, env);
            Some(Stmt::Block(body))
        }
        Stmt::Comptime { body, span } => {
            let body = optimize_stmts(body, &mut env.clone());
            Some(Stmt::Comptime { body, span })
        }
        Stmt::ForEach {
            name,
            elem_type,
            iter,
            body,
            span,
        } => Some(Stmt::ForEach {
            name,
            elem_type,
            iter: fold_expr(&iter, env),
            body: optimize_stmts(body, &mut env.clone()),
            span,
        }),
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => Some(Stmt::CFor {
            init: match init {
                Some(s) => optimize_stmt(*s, &mut env.clone()).map(Box::new),
                None => None,
            },
            test: test.map(|t| fold_expr(&t, env)),
            incr: incr.map(|t| fold_expr(&t, env)),
            body: optimize_stmts(body, &mut env.clone()),
        }),
        Stmt::Spawn { body, parallel } => Some(Stmt::Spawn {
            body: optimize_stmts(body, &mut env.clone()),
            parallel,
        }),
        Stmt::DoWhile { body, test } => Some(Stmt::DoWhile {
            body: optimize_stmts(body, &mut env.clone()),
            test: fold_expr(&test, env),
        }),
        Stmt::Destructure { names, value } => Some(Stmt::Destructure {
            names,
            value: fold_expr(&value, env),
        }),
        Stmt::FieldDestructure { names, value } => Some(Stmt::FieldDestructure {
            names,
            value: fold_expr(&value, env),
        }),
        Stmt::Delay { time, body } => Some(Stmt::Delay {
            time: fold_expr(&time, env),
            body: optimize_stmts(body, &mut env.clone()),
        }),
        Stmt::Defer { body } => Some(Stmt::Defer {
            body: optimize_stmts(body, &mut env.clone()),
        }),
        Stmt::Try {
            body,
            err_name,
            catch,
        } => Some(Stmt::Try {
            body: optimize_stmts(body, &mut env.clone()),
            err_name,
            catch: optimize_stmts(catch, &mut env.clone()),
        }),
        Stmt::Switch {
            discriminant,
            cases,
        } => Some(Stmt::Switch {
            discriminant: fold_expr(&discriminant, env),
            cases: cases
                .into_iter()
                .map(|mut c| {
                    c.values = c.values.iter().map(|v| fold_expr(v, env)).collect();
                    c.body = optimize_stmts(c.body, &mut env.clone());
                    c
                })
                .collect(),
        }),
        Stmt::Match {
            discriminant,
            arms,
        } => Some(Stmt::Match {
            discriminant: fold_expr(&discriminant, env),
            arms: arms
                .into_iter()
                .map(|mut a| {
                    a.body = optimize_stmts(a.body, &mut env.clone());
                    a
                })
                .collect(),
        }),
        other => Some(other),
    }
}
