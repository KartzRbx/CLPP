//! Generic monomorphization for explicit type-arg calls (RFC 0011 §7 lite).

use crate::ast::{Expr, Function, Item, Param, Program, Stmt, TypeParam};
use std::collections::{HashMap, HashSet};

pub fn run(program: &mut Program) -> Vec<String> {
    let generics: HashMap<String, Function> = program
        .items
        .iter()
        .filter_map(|item| match item {
            Item::Function(f) if f.owner.is_none() && !f.type_params.is_empty() => {
                Some((f.name.clone(), f.clone()))
            }
            _ => None,
        })
        .collect();
    if generics.is_empty() {
        return Vec::new();
    }

    let mut needed: HashMap<(String, Vec<String>), ()> = HashMap::new();
    for item in &program.items {
        if let Item::Function(f) | Item::Proto(f) = item {
            collect_instantiations(&f.body, &generics, &mut needed);
        }
    }
    if needed.is_empty() {
        return Vec::new();
    }

    let mut specialized_names = Vec::new();
    let mut new_items = Vec::new();
    let existing: HashSet<String> = program
        .items
        .iter()
        .filter_map(|i| match i {
            Item::Function(f) | Item::Proto(f) => Some(f.name.clone()),
            _ => None,
        })
        .collect();

    for ((base, targs), _) in &needed {
        let Some(generic) = generics.get(base) else {
            continue;
        };
        if targs.len() != generic.type_params.len() {
            continue;
        }
        let spec_name = mangled(base, targs);
        if existing.contains(&spec_name) {
            continue;
        }
        let mut subst = HashMap::new();
        for (tp, concrete) in generic.type_params.iter().zip(targs.iter()) {
            subst.insert(tp.name.clone(), concrete.clone());
        }
        let mut spec = generic.clone();
        spec.name = spec_name.clone();
        spec.type_params = Vec::<TypeParam>::new();
        spec.params = spec
            .params
            .into_iter()
            .map(|p| Param {
                value_type: p.value_type.map(|t| rewrite_type(&t, &subst)),
                ..p
            })
            .collect();
        spec.return_type = spec.return_type.map(|t| rewrite_type(&t, &subst));
        spec.body = rewrite_stmts_types(spec.body, &subst);
        specialized_names.push(spec_name);
        new_items.push(Item::Function(spec));
    }

    // Rewrite call sites to specialized names.
    for item in &mut program.items {
        if let Item::Function(f) | Item::Proto(f) = item {
            f.body = rewrite_calls(std::mem::take(&mut f.body), &generics);
        }
    }

    program.items.extend(new_items);
    specialized_names
}

fn mangled(base: &str, targs: &[String]) -> String {
    let safe: Vec<String> = targs
        .iter()
        .map(|t| {
            t.chars()
                .map(|c| if c.is_ascii_alphanumeric() { c } else { '_' })
                .collect()
        })
        .collect();
    format!("{base}__{}", safe.join("_"))
}

fn collect_instantiations(
    stmts: &[Stmt],
    generics: &HashMap<String, Function>,
    out: &mut HashMap<(String, Vec<String>), ()>,
) {
    for stmt in stmts {
        walk_stmt(stmt, generics, out);
    }
}

fn walk_stmt(
    stmt: &Stmt,
    generics: &HashMap<String, Function>,
    out: &mut HashMap<(String, Vec<String>), ()>,
) {
    match stmt {
        Stmt::Expr(e) | Stmt::Return(Some(e)) => walk_expr(e, generics, out),
        Stmt::Decl(d) => {
            if let Some(v) = &d.value {
                walk_expr(v, generics, out);
            }
        }
        Stmt::If {
            test,
            consequent,
            alternate,
        } => {
            walk_expr(test, generics, out);
            collect_instantiations(consequent, generics, out);
            if let Some(a) = alternate {
                collect_instantiations(a, generics, out);
            }
        }
        Stmt::While { test, body } | Stmt::Guard { test, body } | Stmt::DoWhile { body, test } => {
            walk_expr(test, generics, out);
            collect_instantiations(body, generics, out);
        }
        Stmt::Block(body) | Stmt::Spawn { body, .. } | Stmt::Defer { body } | Stmt::Comptime { body, .. } => {
            collect_instantiations(body, generics, out);
        }
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => {
            if let Some(s) = init {
                walk_stmt(s, generics, out);
            }
            if let Some(t) = test {
                walk_expr(t, generics, out);
            }
            if let Some(t) = incr {
                walk_expr(t, generics, out);
            }
            collect_instantiations(body, generics, out);
        }
        Stmt::ForEach { iter, body, .. } => {
            walk_expr(iter, generics, out);
            collect_instantiations(body, generics, out);
        }
        _ => {}
    }
}

fn walk_expr(
    expr: &Expr,
    generics: &HashMap<String, Function>,
    out: &mut HashMap<(String, Vec<String>), ()>,
) {
    match expr {
        Expr::Call {
            object: None,
            name,
            args,
            type_args,
            ..
        } => {
            if generics.contains_key(name) && !type_args.is_empty() {
                out.insert((name.clone(), type_args.clone()), ());
            }
            for a in args {
                walk_expr(a, generics, out);
            }
        }
        Expr::Call {
            object,
            args,
            ..
        } => {
            if let Some(o) = object {
                walk_expr(o, generics, out);
            }
            for a in args {
                walk_expr(a, generics, out);
            }
        }
        Expr::Unary { argument, .. }
        | Expr::Cast { argument, .. }
        | Expr::Await { argument }
        | Expr::Member { object: argument, .. }
        | Expr::Update { target: argument, .. } => walk_expr(argument, generics, out),
        Expr::Binary { left, right, .. }
        | Expr::Assign { left, right, .. }
        | Expr::Coalesce { left, right }
        | Expr::Index {
            object: left,
            index: right,
        } => {
            walk_expr(left, generics, out);
            walk_expr(right, generics, out);
        }
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => {
            walk_expr(cond, generics, out);
            walk_expr(then_expr, generics, out);
            walk_expr(else_expr, generics, out);
        }
        Expr::Tuple(vs) | Expr::ArrayLit { elements: vs } => {
            for v in vs {
                walk_expr(v, generics, out);
            }
        }
        Expr::InitList { fields } => {
            for (_, v) in fields {
                walk_expr(v, generics, out);
            }
        }
        _ => {}
    }
}

fn rewrite_type(ty: &str, subst: &HashMap<String, String>) -> String {
    // Simple token replace for bare type params (T, U, …).
    let mut out = ty.to_string();
    for (from, to) in subst {
        if out == *from {
            return to.clone();
        }
        // array<T>, optional<T>
        out = out.replace(&format!("<{from}>"), &format!("<{to}>"));
        out = out.replace(&format!("<{from},"), &format!("<{to},"));
        out = out.replace(&format!(", {from}>"), &format!(", {to}>"));
        out = out.replace(&format!(",{from}>"), &format!(",{to}>"));
    }
    out
}

fn rewrite_stmts_types(stmts: Vec<Stmt>, subst: &HashMap<String, String>) -> Vec<Stmt> {
    stmts
        .into_iter()
        .map(|s| rewrite_stmt_types(s, subst))
        .collect()
}

fn rewrite_stmt_types(stmt: Stmt, subst: &HashMap<String, String>) -> Stmt {
    match stmt {
        Stmt::Decl(mut d) => {
            d.value_type = d.value_type.map(|t| rewrite_type(&t, subst));
            Stmt::Decl(d)
        }
        Stmt::Block(body) => Stmt::Block(rewrite_stmts_types(body, subst)),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => Stmt::If {
            test,
            consequent: rewrite_stmts_types(consequent, subst),
            alternate: alternate.map(|a| rewrite_stmts_types(a, subst)),
        },
        Stmt::While { test, body } => Stmt::While {
            test,
            body: rewrite_stmts_types(body, subst),
        },
        other => other,
    }
}

fn rewrite_calls(stmts: Vec<Stmt>, generics: &HashMap<String, Function>) -> Vec<Stmt> {
    stmts
        .into_iter()
        .map(|s| rewrite_call_stmt(s, generics))
        .collect()
}

fn rewrite_call_stmt(stmt: Stmt, generics: &HashMap<String, Function>) -> Stmt {
    match stmt {
        Stmt::Expr(e) => Stmt::Expr(rewrite_call_expr(e, generics)),
        Stmt::Return(Some(e)) => Stmt::Return(Some(rewrite_call_expr(e, generics))),
        Stmt::Decl(mut d) => {
            if let Some(v) = d.value.take() {
                d.value = Some(rewrite_call_expr(v, generics));
            }
            Stmt::Decl(d)
        }
        Stmt::If {
            test,
            consequent,
            alternate,
        } => Stmt::If {
            test: rewrite_call_expr(test, generics),
            consequent: rewrite_calls(consequent, generics),
            alternate: alternate.map(|a| rewrite_calls(a, generics)),
        },
        Stmt::While { test, body } => Stmt::While {
            test: rewrite_call_expr(test, generics),
            body: rewrite_calls(body, generics),
        },
        Stmt::Block(body) => Stmt::Block(rewrite_calls(body, generics)),
        other => other,
    }
}

fn rewrite_call_expr(expr: Expr, generics: &HashMap<String, Function>) -> Expr {
    match expr {
        Expr::Call {
            object: None,
            name,
            args,
            access,
            type_args,
        } if generics.contains_key(&name) && !type_args.is_empty() => Expr::Call {
            object: None,
            name: mangled(&name, &type_args),
            args: args
                .into_iter()
                .map(|a| rewrite_call_expr(a, generics))
                .collect(),
            access,
            type_args: Vec::new(),
        },
        Expr::Call {
            object,
            name,
            args,
            access,
            type_args,
        } => Expr::Call {
            object: object.map(|o| Box::new(rewrite_call_expr(*o, generics))),
            name,
            args: args
                .into_iter()
                .map(|a| rewrite_call_expr(a, generics))
                .collect(),
            access,
            type_args,
        },
        Expr::Unary { op, argument } => Expr::Unary {
            op,
            argument: Box::new(rewrite_call_expr(*argument, generics)),
        },
        Expr::Binary { op, left, right } => Expr::Binary {
            op,
            left: Box::new(rewrite_call_expr(*left, generics)),
            right: Box::new(rewrite_call_expr(*right, generics)),
        },
        other => other,
    }
}
