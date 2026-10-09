use super::{Expr, Item, Program, Stmt};

/// Walk statements and expressions. Return `false` from a callback to stop.
pub fn visit_program(program: &Program, f: &mut impl FnMut(NodeRef<'_>) -> bool) -> bool {
    for item in &program.items {
        if !visit_item(item, f) {
            return false;
        }
    }
    true
}

pub enum NodeRef<'a> {
    Item(&'a Item),
    Stmt(&'a Stmt),
    Expr(&'a Expr),
}

fn visit_item(item: &Item, f: &mut impl FnMut(NodeRef<'_>) -> bool) -> bool {
    if !f(NodeRef::Item(item)) {
        return false;
    }
    match item {
        Item::Function(func) | Item::Proto(func) => {
            for param in &func.params {
                if let Some(default) = &param.default {
                    if !visit_expr(default, f) {
                        return false;
                    }
                }
            }
            visit_stmts(&func.body, f)
        }
        Item::Decl(decl) => {
            if let Some(value) = &decl.value {
                visit_expr(value, f)
            } else {
                true
            }
        }
        Item::Destructure { value, .. } => visit_expr(value, f),
        Item::Unsupported { .. }
        | Item::Enum { .. }
        | Item::TypeAlias { .. }
        | Item::Class { .. }
        | Item::Import { .. } => true,
    }
}

fn visit_stmts(stmts: &[Stmt], f: &mut impl FnMut(NodeRef<'_>) -> bool) -> bool {
    stmts.iter().all(|stmt| visit_stmt(stmt, f))
}

fn visit_stmt(stmt: &Stmt, f: &mut impl FnMut(NodeRef<'_>) -> bool) -> bool {
    if !f(NodeRef::Stmt(stmt)) {
        return false;
    }
    match stmt {
        Stmt::Decl(decl) => decl
            .value
            .as_ref()
            .map(|v| visit_expr(v, f))
            .unwrap_or(true),
        Stmt::Destructure { value, .. } | Stmt::Expr(value) => visit_expr(value, f),
        Stmt::Return(Some(value)) => visit_expr(value, f),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => {
            visit_expr(test, f)
                && visit_stmts(consequent, f)
                && alternate
                    .as_ref()
                    .map(|body| visit_stmts(body, f))
                    .unwrap_or(true)
        }
        Stmt::Guard { test, body } | Stmt::While { test, body } => {
            visit_expr(test, f) && visit_stmts(body, f)
        }
        Stmt::ForEach { iter, body, .. } => visit_expr(iter, f) && visit_stmts(body, f),
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => {
            init.as_ref().map(|s| visit_stmt(s, f)).unwrap_or(true)
                && test.as_ref().map(|e| visit_expr(e, f)).unwrap_or(true)
                && incr.as_ref().map(|e| visit_expr(e, f)).unwrap_or(true)
                && visit_stmts(body, f)
        }
        Stmt::Switch {
            discriminant,
            cases,
        } => {
            visit_expr(discriminant, f)
                && cases.iter().all(|case| {
                    case.values.iter().all(|v| visit_expr(v, f)) && visit_stmts(&case.body, f)
                })
        }
        Stmt::Match { discriminant, arms } => {
            visit_expr(discriminant, f) && arms.iter().all(|arm| visit_stmts(&arm.body, f))
        }
        Stmt::Spawn { body, .. }
        | Stmt::Block(body)
        | Stmt::Defer { body }
        | Stmt::Comptime { body, .. } => visit_stmts(body, f),
        Stmt::DoWhile { body, test } => visit_stmts(body, f) && visit_expr(test, f),
        Stmt::Delay { body, time } => visit_expr(time, f) && visit_stmts(body, f),
        Stmt::Try { body, catch, .. } => visit_stmts(body, f) && visit_stmts(catch, f),
        Stmt::FieldDestructure { value, .. } => visit_expr(value, f),
        Stmt::Return(None) | Stmt::Break | Stmt::Continue => true,
    }
}

fn visit_expr(expr: &Expr, f: &mut impl FnMut(NodeRef<'_>) -> bool) -> bool {
    if !f(NodeRef::Expr(expr)) {
        return false;
    }
    match expr {
        Expr::Unary { argument, .. }
        | Expr::Await { argument }
        | Expr::Cast { argument, .. }
        | Expr::Try { argument }
        | Expr::Update {
            target: argument, ..
        } => visit_expr(argument, f),
        Expr::Member { object, .. } => visit_expr(object, f),
        Expr::Index { object, index } => visit_expr(object, f) && visit_expr(index, f),
        Expr::OptionalChain { object, args, .. } => {
            visit_expr(object, f)
                && args
                    .as_ref()
                    .is_none_or(|args| args.iter().all(|e| visit_expr(e, f)))
        }
        Expr::Coalesce { left, right, .. }
        | Expr::Binary { left, right, .. }
        | Expr::Assign { left, right, .. } => visit_expr(left, f) && visit_expr(right, f),
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => visit_expr(cond, f) && visit_expr(then_expr, f) && visit_expr(else_expr, f),
        Expr::Call { object, args, .. } => {
            object.as_ref().map(|o| visit_expr(o, f)).unwrap_or(true)
                && args.iter().all(|a| visit_expr(a, f))
        }
        Expr::New { args, .. } => args.iter().all(|a| visit_expr(a, f)),
        Expr::Lambda { body, .. } => visit_stmts(body, f),
        Expr::Tuple(values) | Expr::ArrayLit { elements: values } => {
            values.iter().all(|v| visit_expr(v, f))
        }
        Expr::InitList { fields } => fields.iter().all(|(_, v)| visit_expr(v, f)),
        Expr::DictLit { pairs } => pairs
            .iter()
            .all(|(k, v)| visit_expr(k, f) && visit_expr(v, f)),
        Expr::Interp { parts } => parts.iter().all(|part| match part {
            crate::ast::InterpPart::Value(v) => visit_expr(v, f),
            crate::ast::InterpPart::Text(_) => true,
        }),
        _ => true,
    }
}

/// Innermost statement that covers `line`/`col` (1-based).
pub fn stmt_at<'a>(program: &'a Program, line: usize, col: usize) -> Option<&'a Stmt> {
    let mut found = None;
    for item in &program.items {
        match item {
            Item::Function(func) | Item::Proto(func) => {
                walk_stmts(&func.body, line, col, &mut found);
            }
            _ => {}
        }
    }
    found
}

fn walk_stmts<'a>(stmts: &'a [Stmt], line: usize, col: usize, found: &mut Option<&'a Stmt>) {
    for stmt in stmts {
        if stmt.span().contains(line, col) || stmt.span().contains_line(line) {
            *found = Some(stmt);
        }
        match stmt {
            Stmt::If {
                consequent,
                alternate,
                ..
            } => {
                walk_stmts(consequent, line, col, found);
                if let Some(alt) = alternate {
                    walk_stmts(alt, line, col, found);
                }
            }
            Stmt::Guard { body, .. }
            | Stmt::While { body, .. }
            | Stmt::ForEach { body, .. }
            | Stmt::Spawn { body, .. }
            | Stmt::Block(body)
            | Stmt::CFor { body, .. }
            | Stmt::DoWhile { body, .. }
            | Stmt::Delay { body, .. }
            | Stmt::Defer { body, .. }
            | Stmt::Comptime { body, .. } => walk_stmts(body, line, col, found),
            Stmt::Try { body, catch, .. } => {
                walk_stmts(body, line, col, found);
                walk_stmts(catch, line, col, found);
            }
            Stmt::Switch { cases, .. } => {
                for case in cases {
                    walk_stmts(&case.body, line, col, found);
                }
            }
            Stmt::Match { arms, .. } => {
                for arm in arms {
                    walk_stmts(&arm.body, line, col, found);
                }
            }
            _ => {}
        }
    }
}

/// Whether propagation belongs to this expression's scope (lambda bodies are separate).
pub fn has_try(expr: &Expr) -> bool {
    match expr {
        Expr::Try { .. } => true,
        Expr::Lambda { .. } => false,
        Expr::Unary { argument, .. }
        | Expr::Await { argument }
        | Expr::Cast { argument, .. }
        | Expr::Update {
            target: argument, ..
        } => has_try(argument),
        Expr::Member { object, .. } => has_try(object),
        Expr::Index { object, index } => has_try(object) || has_try(index),
        Expr::OptionalChain { object, args, .. } => {
            has_try(object) || args.as_ref().is_some_and(|args| args.iter().any(has_try))
        }
        Expr::Binary { left, right, .. }
        | Expr::Coalesce { left, right }
        | Expr::Assign { left, right, .. } => has_try(left) || has_try(right),
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => has_try(cond) || has_try(then_expr) || has_try(else_expr),
        Expr::Call { object, args, .. } => {
            object.as_ref().is_some_and(|e| has_try(e)) || args.iter().any(has_try)
        }
        Expr::New { args, .. } | Expr::Tuple(args) | Expr::ArrayLit { elements: args } => {
            args.iter().any(has_try)
        }
        Expr::InitList { fields } => fields.iter().any(|(_, e)| has_try(e)),
        Expr::DictLit { pairs } => pairs.iter().any(|(k, v)| has_try(k) || has_try(v)),
        Expr::Interp { parts } => parts
            .iter()
            .any(|p| matches!(p, super::InterpPart::Value(e) if has_try(e))),
        _ => false,
    }
}

pub fn stmt_has_try(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Decl(d) => d.value.as_ref().is_some_and(has_try),
        Stmt::Destructure { value, .. }
        | Stmt::FieldDestructure { value, .. }
        | Stmt::Expr(value)
        | Stmt::Return(Some(value)) => has_try(value),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => {
            has_try(test)
                || consequent.iter().any(stmt_has_try)
                || alternate
                    .as_ref()
                    .is_some_and(|b| b.iter().any(stmt_has_try))
        }
        Stmt::Guard { test, body } | Stmt::While { test, body } | Stmt::DoWhile { test, body } => {
            has_try(test) || body.iter().any(stmt_has_try)
        }
        Stmt::ForEach { iter, body, .. } => has_try(iter) || body.iter().any(stmt_has_try),
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => {
            init.as_ref().is_some_and(|s| stmt_has_try(s))
                || test.as_ref().is_some_and(has_try)
                || incr.as_ref().is_some_and(has_try)
                || body.iter().any(stmt_has_try)
        }
        Stmt::Switch {
            discriminant,
            cases,
        } => {
            has_try(discriminant)
                || cases
                    .iter()
                    .any(|c| c.values.iter().any(has_try) || c.body.iter().any(stmt_has_try))
        }
        Stmt::Match { discriminant, arms } => {
            has_try(discriminant) || arms.iter().any(|a| a.body.iter().any(stmt_has_try))
        }
        Stmt::Block(body) | Stmt::Comptime { body, .. } => body.iter().any(stmt_has_try),
        Stmt::Try { body, catch, .. } => {
            body.iter().any(stmt_has_try) || catch.iter().any(stmt_has_try)
        }
        Stmt::Delay { time, .. } => has_try(time),
        _ => false,
    }
}
