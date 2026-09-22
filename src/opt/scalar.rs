//! Scalar replacement of non-escaping InitList / Vector temps (RFC 0011 §5 / escape lite).

use crate::ast::{Decl, Expr, Item, Program, Span, Stmt};
use crate::opt::escape::{is_scalarizable_new, zip_vector_args};
use std::collections::HashMap;

pub fn run(program: &mut Program) {
    for item in &mut program.items {
        if let Item::Function(func) | Item::Proto(func) = item {
            func.body = rewrite_body(std::mem::take(&mut func.body));
        }
    }
}

fn rewrite_body(stmts: Vec<Stmt>) -> Vec<Stmt> {
    let mut field_maps: HashMap<String, HashMap<String, String>> = HashMap::new();
    let mut inits: HashMap<String, (Vec<(String, Expr)>, Decl)> = HashMap::new();

    for stmt in &stmts {
        if let Stmt::Decl(decl) = stmt {
            if name_escapes(&decl.name, &stmts) {
                continue;
            }
            let fields_opt = match &decl.value {
                Some(Expr::InitList { fields })
                    if !fields.is_empty() && fields.iter().all(|(n, _)| !n.is_empty()) =>
                {
                    Some(fields.clone())
                }
                Some(Expr::New { class_name, args }) if is_scalarizable_new(class_name) => {
                    zip_vector_args(class_name, args)
                }
                _ => None,
            };
            if let Some(fields) = fields_opt {
                let mut map = HashMap::new();
                for (field, _) in &fields {
                    map.insert(field.clone(), format!("{}__{}", decl.name, field));
                }
                field_maps.insert(decl.name.clone(), map);
                inits.insert(decl.name.clone(), (fields, decl.clone()));
            }
        }
    }

    if field_maps.is_empty() {
        return stmts;
    }

    let mut out = Vec::new();
    for stmt in stmts {
        match stmt {
            Stmt::Decl(decl) if inits.contains_key(&decl.name) => {
                let (fields, meta) = inits.get(&decl.name).unwrap();
                let map = field_maps.get(&decl.name).unwrap();
                for (field, value) in fields {
                    let local = map.get(field).unwrap().clone();
                    out.push(Stmt::Decl(Decl {
                        name: local,
                        value_type: None,
                        value: Some(rewrite_expr(value.clone(), &field_maps)),
                        is_const: meta.is_const,
                        is_observable: false,
                        owner: None,
                        line: meta.line,
                        span: Span::default(),
                        doc: None,
                        visibility: None,
                    }));
                }
            }
            other => out.push(rewrite_stmt(other, &field_maps)),
        }
    }
    out
}

fn name_escapes(name: &str, stmts: &[Stmt]) -> bool {
    for stmt in stmts {
        if stmt_uses_whole(name, stmt) {
            return true;
        }
    }
    false
}

fn stmt_uses_whole(name: &str, stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Decl(decl) => {
            if decl.name == name {
                return false;
            }
            decl.value
                .as_ref()
                .is_some_and(|v| expr_uses_whole(name, v))
        }
        Stmt::Expr(e) | Stmt::Return(Some(e)) => expr_uses_whole(name, e),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => {
            expr_uses_whole(name, test)
                || consequent.iter().any(|s| stmt_uses_whole(name, s))
                || alternate
                    .as_ref()
                    .is_some_and(|a| a.iter().any(|s| stmt_uses_whole(name, s)))
        }
        Stmt::While { test, body } | Stmt::DoWhile { body, test } | Stmt::Guard { test, body } => {
            expr_uses_whole(name, test) || body.iter().any(|s| stmt_uses_whole(name, s))
        }
        Stmt::Block(body) | Stmt::Spawn { body, .. } | Stmt::Defer { body } | Stmt::Comptime { body, .. } => {
            body.iter().any(|s| stmt_uses_whole(name, s))
        }
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => {
            init.as_ref().is_some_and(|s| stmt_uses_whole(name, s))
                || test.as_ref().is_some_and(|t| expr_uses_whole(name, t))
                || incr.as_ref().is_some_and(|t| expr_uses_whole(name, t))
                || body.iter().any(|s| stmt_uses_whole(name, s))
        }
        Stmt::ForEach { iter, body, .. } => {
            expr_uses_whole(name, iter) || body.iter().any(|s| stmt_uses_whole(name, s))
        }
        Stmt::Destructure { value, .. } | Stmt::FieldDestructure { value, .. } => {
            expr_uses_whole(name, value)
        }
        Stmt::Delay { time, body } => {
            expr_uses_whole(name, time) || body.iter().any(|s| stmt_uses_whole(name, s))
        }
        Stmt::Try { body, catch, .. } => {
            body.iter().any(|s| stmt_uses_whole(name, s))
                || catch.iter().any(|s| stmt_uses_whole(name, s))
        }
        Stmt::Switch {
            discriminant,
            cases,
        } => {
            expr_uses_whole(name, discriminant)
                || cases.iter().any(|c| {
                    c.values.iter().any(|v| expr_uses_whole(name, v))
                        || c.body.iter().any(|s| stmt_uses_whole(name, s))
                })
        }
        Stmt::Match {
            discriminant,
            arms,
        } => {
            expr_uses_whole(name, discriminant)
                || arms.iter().any(|a| a.body.iter().any(|s| stmt_uses_whole(name, s)))
        }
        _ => false,
    }
}

/// True if `name` appears as a whole value (not only as `name.field`).
fn expr_uses_whole(name: &str, expr: &Expr) -> bool {
    match expr {
        Expr::Ident(n) => n == name,
        Expr::Member { object, .. } => {
            // object.field is OK if object is Ident(name); recurse into nested.
            match object.as_ref() {
                Expr::Ident(n) if n == name => false,
                _ => expr_uses_whole(name, object),
            }
        }
        Expr::Unary { argument, .. }
        | Expr::Await { argument }
        | Expr::Cast { argument, .. }
        | Expr::Update { target: argument, .. } => expr_uses_whole(name, argument),
        Expr::Binary { left, right, .. }
        | Expr::Assign { left, right, .. }
        | Expr::Coalesce { left, right }
        | Expr::Index {
            object: left,
            index: right,
        } => expr_uses_whole(name, left) || expr_uses_whole(name, right),
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => {
            expr_uses_whole(name, cond)
                || expr_uses_whole(name, then_expr)
                || expr_uses_whole(name, else_expr)
        }
        Expr::Call {
            object,
            args,
            ..
        } => {
            object
                .as_ref()
                .is_some_and(|o| expr_uses_whole(name, o))
                || args.iter().any(|a| expr_uses_whole(name, a))
        }
        Expr::New { args, .. } => args.iter().any(|a| expr_uses_whole(name, a)),
        Expr::Tuple(values) | Expr::ArrayLit { elements: values } => {
            values.iter().any(|v| expr_uses_whole(name, v))
        }
        Expr::InitList { fields } => fields.iter().any(|(_, v)| expr_uses_whole(name, v)),
        Expr::DictLit { pairs } => pairs
            .iter()
            .any(|(k, v)| expr_uses_whole(name, k) || expr_uses_whole(name, v)),
        Expr::OptionalChain { object, args, .. } => {
            expr_uses_whole(name, object)
                || args
                    .as_ref()
                    .is_some_and(|a| a.iter().any(|e| expr_uses_whole(name, e)))
        }
        Expr::Lambda { .. } | Expr::Interp { .. } | Expr::Null | Expr::Bool(_) | Expr::Number(_)
        | Expr::String(_) | Expr::This { .. } | Expr::AtField { .. } => false,
    }
}

fn rewrite_stmt(stmt: Stmt, maps: &HashMap<String, HashMap<String, String>>) -> Stmt {
    match stmt {
        Stmt::Decl(mut decl) => {
            if let Some(v) = decl.value.take() {
                decl.value = Some(rewrite_expr(v, maps));
            }
            Stmt::Decl(decl)
        }
        Stmt::Expr(e) => Stmt::Expr(rewrite_expr(e, maps)),
        Stmt::Return(Some(e)) => Stmt::Return(Some(rewrite_expr(e, maps))),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => Stmt::If {
            test: rewrite_expr(test, maps),
            consequent: consequent
                .into_iter()
                .map(|s| rewrite_stmt(s, maps))
                .collect(),
            alternate: alternate.map(|a| {
                a.into_iter()
                    .map(|s| rewrite_stmt(s, maps))
                    .collect()
            }),
        },
        Stmt::While { test, body } => Stmt::While {
            test: rewrite_expr(test, maps),
            body: body.into_iter().map(|s| rewrite_stmt(s, maps)).collect(),
        },
        Stmt::Block(body) => Stmt::Block(body.into_iter().map(|s| rewrite_stmt(s, maps)).collect()),
        other => other,
    }
}

fn rewrite_expr(expr: Expr, maps: &HashMap<String, HashMap<String, String>>) -> Expr {
    match expr {
        Expr::Member {
            object,
            name,
            access,
        } => {
            if let Expr::Ident(base) = object.as_ref() {
                if let Some(fields) = maps.get(base) {
                    if let Some(local) = fields.get(&name) {
                        return Expr::Ident(local.clone());
                    }
                }
            }
            Expr::Member {
                object: Box::new(rewrite_expr(*object, maps)),
                name,
                access,
            }
        }
        Expr::Unary { op, argument } => Expr::Unary {
            op,
            argument: Box::new(rewrite_expr(*argument, maps)),
        },
        Expr::Binary { op, left, right } => Expr::Binary {
            op,
            left: Box::new(rewrite_expr(*left, maps)),
            right: Box::new(rewrite_expr(*right, maps)),
        },
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => Expr::Ternary {
            cond: Box::new(rewrite_expr(*cond, maps)),
            then_expr: Box::new(rewrite_expr(*then_expr, maps)),
            else_expr: Box::new(rewrite_expr(*else_expr, maps)),
        },
        Expr::Call {
            object,
            name,
            args,
            access,
            type_args,
        } => Expr::Call {
            object: object.map(|o| Box::new(rewrite_expr(*o, maps))),
            name,
            args: args.into_iter().map(|a| rewrite_expr(a, maps)).collect(),
            access,
            type_args,
        },
        Expr::Assign {
            op,
            left,
            right,
            line,
        } => Expr::Assign {
            op,
            left: Box::new(rewrite_expr(*left, maps)),
            right: Box::new(rewrite_expr(*right, maps)),
            line,
        },
        Expr::Index { object, index } => Expr::Index {
            object: Box::new(rewrite_expr(*object, maps)),
            index: Box::new(rewrite_expr(*index, maps)),
        },
        Expr::Cast {
            value_type,
            argument,
            kind,
        } => Expr::Cast {
            value_type,
            argument: Box::new(rewrite_expr(*argument, maps)),
            kind,
        },
        Expr::InitList { fields } => Expr::InitList {
            fields: fields
                .into_iter()
                .map(|(n, v)| (n, rewrite_expr(v, maps)))
                .collect(),
        },
        other => other,
    }
}
