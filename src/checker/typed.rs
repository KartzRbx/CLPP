//! TypeId-based validation (assignment, calls, returns, members). Runs after `resolve`.

use crate::ast::{Expr, Function, Item, Program, Stmt};
use crate::binder::BoundFile;
use crate::builtins;
use crate::diag;
use crate::names::{is_bare_global, is_datatype};
use crate::support::CompileDiagnostic;
use crate::types::{parse_type_str, MemberKind, TypeDatabase, TypeId};
use std::collections::{HashMap, HashSet};

use super::Engine;

struct CallSig {
    label: String,
    params: Vec<(String, TypeId)>,
    min_params: usize,
    #[allow(dead_code)]
    ret: TypeId,
}

fn collect_private(program: &Program) -> HashMap<String, HashSet<String>> {
    let mut map: HashMap<String, HashSet<String>> = HashMap::new();
    for item in &program.items {
        match item {
            Item::Decl(decl) if decl.visibility.as_deref() == Some("private") => {
                if let Some(owner) = &decl.owner {
                    map.entry(owner.clone())
                        .or_default()
                        .insert(decl.name.clone());
                }
            }
            Item::Function(func) | Item::Proto(func)
                if func.visibility.as_deref() == Some("private") =>
            {
                if let Some(owner) = &func.owner {
                    map.entry(owner.clone())
                        .or_default()
                        .insert(func.name.clone());
                }
            }
            _ => {}
        }
    }
    map
}

/// Extra diagnostics from interned types. Complements the name-table pass.
pub fn check_typed(
    program: &Program,
    bound: &BoundFile,
    types: &mut TypeDatabase,
    source: &str,
) -> Vec<CompileDiagnostic> {
    let mut engine = Engine {
        program,
        symbols: &bound.symbols,
        types,
        source,
        private: collect_private(program),
    };
    let mut out = Vec::new();
    for item in &program.items {
        match item {
            Item::Function(func) => check_function(&mut engine, func, &mut out),
            Item::Decl(decl) if decl.owner.is_none() => {
                if let (Some(ty), Some(value)) = (&decl.value_type, &decl.value) {
                    let expected = parse_type_str(engine.types, ty);
                    let actual = engine.type_of_expr(value, decl.line);
                    push_assign(
                        &mut out,
                        decl.line,
                        expected,
                        actual,
                        engine.types,
                        AssignSite::Init {
                            name: &decl.name,
                            is_const: decl.is_const,
                        },
                    );
                }
                if let Some(value) = &decl.value {
                    check_expr(&mut engine, value, decl.line, None, false, 0, false, &mut out);
                }
            }
            _ => {}
        }
    }
    out
}

fn check_function(engine: &mut Engine<'_>, func: &Function, out: &mut Vec<CompileDiagnostic>) {
    let expected_ret = func
        .return_type
        .as_deref()
        .filter(|t| !t.is_empty() && *t != "void")
        .map(|t| parse_type_str(engine.types, t));
    check_return_paths(func, expected_ret.is_some(), out);
    let method_owner = func.owner.as_deref();
    walk_stmts(
        engine,
        &func.body,
        expected_ret,
        func.line.max(1),
        method_owner,
        false,
        0,
        false,
        out,
    );
}

fn short_member_name(name: &str) -> &str {
    name.rsplit("::").next().unwrap_or(name)
}

fn in_method(engine: &Engine<'_>, line: usize, method_owner: Option<&str>) -> bool {
    method_owner.is_some() || engine.symbols.enclosing_owner(line).is_some()
}

fn resolve_owner_id(
    engine: &Engine<'_>,
    line: usize,
    method_owner: Option<&str>,
) -> Option<crate::symbols::SymbolId> {
    if let Some(name) = method_owner {
        if let Some(id) = engine.symbols.struct_named(name) {
            return Some(id);
        }
    }
    engine.symbols.enclosing_owner(line)
}

fn local_binding(engine: &Engine<'_>, line: usize, name: &str) -> bool {
    let Some(id) = engine.symbols.lookup_at(line, name) else {
        return false;
    };
    let Some(sym) = engine.symbols.get(id) else {
        return false;
    };
    matches!(
        sym.kind,
        crate::symbols::SymbolKind::Variable | crate::symbols::SymbolKind::Parameter
    )
}

fn owner_member_kind(
    engine: &Engine<'_>,
    owner: crate::symbols::SymbolId,
    name: &str,
) -> Option<crate::symbols::SymbolKind> {
    for mid in engine.symbols.members_of(owner) {
        let Some(sym) = engine.symbols.get(mid) else {
            continue;
        };
        if short_member_name(&sym.name) == name {
            return Some(sym.kind);
        }
    }
    let Some(owner_sym) = engine.symbols.get(owner) else {
        return None;
    };
    let ty = engine.types.struct_info(&owner_sym.name);
    let Some(info) = ty else {
        return None;
    };
    info.members.iter().find_map(|m| {
        if m.name == name {
            Some(match m.kind {
                MemberKind::Field | MemberKind::Property => crate::symbols::SymbolKind::Field,
                MemberKind::Method | MemberKind::Constructor => crate::symbols::SymbolKind::Method,
            })
        } else {
            None
        }
    })
}

fn owner_member_names(engine: &Engine<'_>, owner: crate::symbols::SymbolId) -> Vec<String> {
    let mut names: Vec<String> = engine
        .symbols
        .members_of(owner)
        .into_iter()
        .filter_map(|id| engine.symbols.get(id).map(|s| short_member_name(&s.name).to_string()))
        .collect();
    if let Some(owner_sym) = engine.symbols.get(owner) {
        if let Some(info) = engine.types.struct_info(&owner_sym.name) {
            for m in &info.members {
                if !names.iter().any(|n| n == &m.name) {
                    names.push(m.name.clone());
                }
            }
        }
    }
    names
}

/// Bare field `n` inside `Class::Method` → CLPP0102 (needs `@n`).
fn check_bare_field(
    engine: &Engine<'_>,
    name: &str,
    line: usize,
    method_owner: Option<&str>,
    out: &mut Vec<CompileDiagnostic>,
) -> bool {
    if local_binding(engine, line, name) {
        return false;
    }
    let Some(owner) = resolve_owner_id(engine, line, method_owner) else {
        return false;
    };
    match owner_member_kind(engine, owner, name) {
        Some(crate::symbols::SymbolKind::Field | crate::symbols::SymbolKind::Property) => {
            out.push(diag::diag(
                diag::CLPP0102,
                line.max(1),
                1,
                format!("field `{name}` needs `@`"),
                "error",
            ));
            true
        }
        _ => false,
    }
}

/// Bare `B(…)` inside method when `B` is a method of `self` → CLPP0101.
fn check_bare_method(
    engine: &Engine<'_>,
    name: &str,
    line: usize,
    method_owner: Option<&str>,
    out: &mut Vec<CompileDiagnostic>,
) -> bool {
    if local_binding(engine, line, name) {
        return false;
    }
    let Some(owner) = resolve_owner_id(engine, line, method_owner) else {
        return false;
    };
    match owner_member_kind(engine, owner, name) {
        Some(crate::symbols::SymbolKind::Method | crate::symbols::SymbolKind::Constructor) => {
            out.push(diag::diag(
                diag::CLPP0101,
                line.max(1),
                1,
                format!("method `{name}` needs `@`"),
                "error",
            ));
            true
        }
        _ => false,
    }
}

fn check_at_receiver(
    engine: &Engine<'_>,
    name: Option<&str>,
    line: usize,
    method_owner: Option<&str>,
    out: &mut Vec<CompileDiagnostic>,
) {
    if !in_method(engine, line, method_owner) {
        let msg = match name {
            None | Some("this") | Some("self") => {
                "`@this` is only valid inside Class::Method".into()
            }
            Some(n) => format!("`@{n}` is only valid inside Class::Method"),
        };
        out.push(CompileDiagnostic {
            message: msg,
            line: line.max(1),
            column: 1,
            severity: "error".into(),
            code: None,
            help: None,
        });
        return;
    }
    let Some(n) = name.filter(|n| *n != "this" && *n != "self") else {
        return;
    };
    let Some(owner) = resolve_owner_id(engine, line, method_owner) else {
        return;
    };
    if owner_member_kind(engine, owner, n).is_some() {
        return;
    }
    let candidates = owner_member_names(engine, owner);
    let extra = diag::did_you_mean(n, candidates.iter().map(|s| s.as_str()))
        .map(|h| format!("; did you mean `@{h}`?"))
        .unwrap_or_default();
    out.push(CompileDiagnostic {
        message: format!("`@{n}` is not a field or method of this struct{extra}"),
        line: line.max(1),
        column: 1,
        severity: "error".into(),
        code: None,
        help: None,
    });
}

/// Bare `return;` and missing value on some path (was `pass::check_returns`).
fn check_return_paths(func: &Function, needs_value: bool, out: &mut Vec<CompileDiagnostic>) {
    if !needs_value {
        return;
    }
    let ty = func.return_type.as_deref().unwrap_or("a value");
    if has_bare_return(&func.body) {
        out.push(CompileDiagnostic {
            message: format!("`{}` returns {ty} — `return;` has no value", func.name),
            line: func.line.max(1),
            column: 1,
            severity: "error".into(),
            code: None,
            help: None,
        });
    }
    if !always_returns_value(&func.body) {
        out.push(CompileDiagnostic {
            message: format!("`{}` must return {ty} on every path", func.name),
            line: func.line.max(1),
            column: 1,
            severity: "error".into(),
            code: None,
            help: None,
        });
    }
}

fn has_bare_return(stmts: &[Stmt]) -> bool {
    stmts.iter().any(|stmt| match stmt {
        Stmt::Return(None) => true,
        Stmt::Return(_) => false,
        Stmt::If {
            consequent,
            alternate,
            ..
        } => {
            has_bare_return(consequent)
                || alternate.as_ref().is_some_and(|a| has_bare_return(a))
        }
        Stmt::Guard { body, .. }
        | Stmt::While { body, .. }
        | Stmt::ForEach { body, .. }
        | Stmt::Spawn { body, .. }
        | Stmt::Block(body) => has_bare_return(body),
        Stmt::CFor { body, init, .. } => {
            has_bare_return(body)
                || init
                    .as_ref()
                    .is_some_and(|s| has_bare_return(std::slice::from_ref(s)))
        }
        Stmt::Match { arms, .. } => arms.iter().any(|a| has_bare_return(&a.body)),
        Stmt::Switch { cases, .. } => cases.iter().any(|c| has_bare_return(&c.body)),
        _ => false,
    })
}

fn always_returns_value(stmts: &[Stmt]) -> bool {
    let mut ok = false;
    for stmt in stmts {
        match stmt {
            Stmt::Return(Some(_)) => return true,
            Stmt::Return(None) => return false,
            Stmt::If {
                consequent,
                alternate,
                ..
            } => {
                if let Some(alt) = alternate {
                    if always_returns_value(consequent) && always_returns_value(alt) {
                        ok = true;
                    }
                }
            }
            Stmt::Block(body) | Stmt::Spawn { body, .. } => {
                if always_returns_value(body) {
                    ok = true;
                }
            }
            Stmt::Match { arms, .. } => {
                if !arms.is_empty() && arms.iter().all(|a| always_returns_value(&a.body)) {
                    ok = true;
                }
            }
            _ => {}
        }
    }
    ok
}

fn walk_stmts(
    engine: &mut Engine<'_>,
    stmts: &[Stmt],
    expected_ret: Option<TypeId>,
    mut line: usize,
    method_owner: Option<&str>,
    comptime: bool,
    loop_depth: usize,
    in_do_while: bool,
    out: &mut Vec<CompileDiagnostic>,
) {
    for stmt in stmts {
        let span = stmt.span();
        if span.start_line > 0 {
            line = span.start_line;
        }
        match stmt {
            Stmt::Decl(decl) => {
                line = decl.line.max(line);
                if let (Some(ty), Some(value)) = (&decl.value_type, &decl.value) {
                    let expected = parse_type_str(engine.types, ty);
                    let actual = engine.type_of_expr(value, line);
                    push_assign(
                        out,
                        line,
                        expected,
                        actual,
                        engine.types,
                        AssignSite::Init {
                            name: &decl.name,
                            is_const: decl.is_const,
                        },
                    );
                }
                if let Some(value) = &decl.value {
                    check_expr(
                        engine,
                        value,
                        line,
                        method_owner,
                        comptime,
                        loop_depth,
                        in_do_while,
                        out,
                    );
                }
            }
            Stmt::Expr(expr) => {
                if let Expr::Assign {
                    left,
                    right,
                    line: al,
                    ..
                } = expr
                {
                    line = (*al).max(line);
                    let expected = engine.type_of_expr(left, line);
                    let actual = engine.type_of_expr(right, line);
                    push_assign(out, line, expected, actual, engine.types, AssignSite::Flow);
                }
                check_expr(
                    engine,
                    expr,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
            Stmt::Return(Some(expr)) => {
                let actual = engine.type_of_expr(expr, line);
                if let Some(expected) = expected_ret {
                    push_assign(out, line, expected, actual, engine.types, AssignSite::Flow);
                }
                check_expr(
                    engine,
                    expr,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
            Stmt::Return(None) => {}
            Stmt::Break => {
                if loop_depth == 0 {
                    out.push(CompileDiagnostic {
                        message: "`break` is only valid inside a loop".into(),
                        line: line.max(1),
                        column: 1,
                        severity: "error".into(),
                        code: None,
                        help: None,
                    });
                }
            }
            Stmt::Continue => {
                if loop_depth == 0 {
                    out.push(diag::diag(
                        diag::CLPP0402,
                        line.max(1),
                        1,
                        "continue is only valid inside a loop",
                        "error",
                    ));
                } else if in_do_while {
                    out.push(diag::diag(
                        diag::CLPP0402,
                        line.max(1),
                        1,
                        "continue is not allowed in do/while",
                        "error",
                    ));
                }
            }
            Stmt::If {
                test,
                consequent,
                alternate,
                ..
            } => {
                check_expr(
                    engine,
                    test,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
                walk_stmts(
                    engine,
                    consequent,
                    expected_ret,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
                if let Some(alt) = alternate {
                    walk_stmts(
                        engine,
                        alt,
                        expected_ret,
                        line,
                        method_owner,
                        comptime,
                        loop_depth,
                        in_do_while,
                        out,
                    );
                }
            }
            Stmt::Guard { test, body, .. } => {
                check_expr(
                    engine,
                    test,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
                walk_stmts(
                    engine,
                    body,
                    expected_ret,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
            Stmt::While { test, body, .. } => {
                check_expr(
                    engine,
                    test,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
                walk_stmts(
                    engine,
                    body,
                    expected_ret,
                    line,
                    method_owner,
                    comptime,
                    loop_depth + 1,
                    false,
                    out,
                );
            }
            Stmt::DoWhile { body, test, .. } => {
                walk_stmts(
                    engine,
                    body,
                    expected_ret,
                    line,
                    method_owner,
                    comptime,
                    loop_depth + 1,
                    true,
                    out,
                );
                check_expr(
                    engine,
                    test,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
            Stmt::ForEach { iter, body, .. } => {
                check_expr(
                    engine,
                    iter,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
                walk_stmts(
                    engine,
                    body,
                    expected_ret,
                    line,
                    method_owner,
                    comptime,
                    loop_depth + 1,
                    false,
                    out,
                );
            }
            Stmt::Block(body) | Stmt::Spawn { body, .. } => {
                walk_stmts(
                    engine,
                    body,
                    expected_ret,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
            Stmt::Comptime { body, .. } => {
                walk_stmts(
                    engine,
                    body,
                    None,
                    line,
                    method_owner,
                    true,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
            Stmt::CFor {
                init,
                test,
                incr,
                body,
            } => {
                if let Some(init) = init {
                    walk_stmts(
                        engine,
                        std::slice::from_ref(init),
                        expected_ret,
                        line,
                        method_owner,
                        comptime,
                        loop_depth,
                        in_do_while,
                        out,
                    );
                }
                if let Some(test) = test {
                    check_expr(
                        engine,
                        test,
                        line,
                        method_owner,
                        comptime,
                        loop_depth,
                        in_do_while,
                        out,
                    );
                }
                if let Some(incr) = incr {
                    check_expr(
                        engine,
                        incr,
                        line,
                        method_owner,
                        comptime,
                        loop_depth,
                        in_do_while,
                        out,
                    );
                }
                walk_stmts(
                    engine,
                    body,
                    expected_ret,
                    line,
                    method_owner,
                    comptime,
                    loop_depth + 1,
                    false,
                    out,
                );
            }
            Stmt::Destructure { value, .. } => {
                check_expr(
                    engine,
                    value,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
            _ => {}
        }
    }
}

fn check_expr(
    engine: &mut Engine<'_>,
    expr: &Expr,
    line: usize,
    method_owner: Option<&str>,
    comptime: bool,
    loop_depth: usize,
    in_do_while: bool,
    out: &mut Vec<CompileDiagnostic>,
) {
    match expr {
        Expr::Call {
            object,
            name,
            args,
            type_args,
            ..
        } => {
            if name == "static_assert" {
                check_static_assert(args, line, out);
            }
            if comptime && crate::intent::is_reflect_builtin(name) {
                check_reflect_call(engine, name, args, line, out);
            } else if !comptime && crate::intent::is_reflect_builtin(name) && object.is_none() {
                out.push(CompileDiagnostic {
                    message: format!("`{name}` is only valid inside `comptime {{ ... }}`"),
                    line: line.max(1),
                    column: 1,
                    severity: "error".into(),
                    code: None,
                    help: None,
                });
            }
            if name == "GetService" {
                check_getservice(engine, type_args, args, line, out);
            }
            if let Some(object) = object {
                check_member_name(engine, object, name, line, method_owner, out, true);
                check_expr(
                    engine,
                    object,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            } else if builtins::find(name).is_some() && !builtins::arity_ok(name, args.len()) {
                out.push(CompileDiagnostic {
                    message: format!("wrong number of arguments to `{name}`"),
                    line: line.max(1),
                    column: 1,
                    severity: "error".into(),
                    code: None,
                    help: None,
                });
            } else if builtins::find(name).is_none()
                && name != "static_assert"
                && !crate::intent::is_reflect_builtin(name)
            {
                if !check_bare_method(engine, name, line, method_owner, out) {
                    check_unknown_ident(engine, name, line, method_owner, out);
                }
            }
            if let Some(sig) = resolve_call_sig(engine, object.as_deref(), name, line) {
                check_call_args(engine, &sig, args, line, out);
            }
            if object.is_none() {
                if let Some(func) = find_free_function(engine.program, name) {
                    check_generic_call_bounds(engine, func, type_args, args, line, out);
                }
            }
            for arg in args {
                check_expr(
                    engine,
                    arg,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
        }
        Expr::Member { object, name, .. } => {
            check_member_name(engine, object, name, line, method_owner, out, false);
            check_expr(
                engine,
                object,
                line,
                method_owner,
                comptime,
                loop_depth,
                in_do_while,
                out,
            );
        }
        Expr::Assign { left, right, line, .. } => {
            check_const_assign(engine, left, *line, out);
            check_expr(
                engine,
                left,
                *line,
                method_owner,
                comptime,
                loop_depth,
                in_do_while,
                out,
            );
            check_expr(
                engine,
                right,
                *line,
                method_owner,
                comptime,
                loop_depth,
                in_do_while,
                out,
            );
        }
        Expr::Ident(name) => {
            if name == "this" || name == "self" {
                if !in_method(engine, line, method_owner) {
                    out.push(CompileDiagnostic {
                        message: "`this` is only valid inside Class::Method".into(),
                        line: line.max(1),
                        column: 1,
                        severity: "error".into(),
                        code: None,
                        help: None,
                    });
                }
            } else if !check_bare_field(engine, name, line, method_owner, out) {
                check_unknown_ident(engine, name, line, method_owner, out);
            }
        }
        Expr::This { line: l } => {
            check_at_receiver(engine, Some("this"), (*l).max(line), method_owner, out);
        }
        Expr::AtField { name, line: l } => {
            check_at_receiver(engine, Some(name), (*l).max(line), method_owner, out);
        }
        Expr::Binary { left, right, .. }
        | Expr::Coalesce { left, right }
        | Expr::Index {
            object: left,
            index: right,
        } => {
            check_expr(
                engine,
                left,
                line,
                method_owner,
                comptime,
                loop_depth,
                in_do_while,
                out,
            );
            check_expr(
                engine,
                right,
                line,
                method_owner,
                comptime,
                loop_depth,
                in_do_while,
                out,
            );
        }
        Expr::Unary { argument, .. }
        | Expr::Await { argument }
        | Expr::Cast { argument, .. }
        | Expr::Update { target: argument, .. } => {
            check_expr(
                engine,
                argument,
                line,
                method_owner,
                comptime,
                loop_depth,
                in_do_while,
                out,
            )
        }
        Expr::New { args, class_name, .. } => {
            if let Some(sig) = resolve_ctor_sig(engine, class_name) {
                check_call_args(engine, &sig, args, line, out);
            }
            for a in args {
                check_expr(
                    engine,
                    a,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
        }
        Expr::Tuple(args) | Expr::ArrayLit { elements: args } => {
            for a in args {
                check_expr(
                    engine,
                    a,
                    line,
                    method_owner,
                    comptime,
                    loop_depth,
                    in_do_while,
                    out,
                );
            }
        }
        Expr::Lambda { body, .. } => {
            walk_stmts(
                engine,
                body,
                None,
                line,
                method_owner,
                comptime,
                loop_depth,
                in_do_while,
                out,
            );
        }
        _ => {}
    }
}

fn check_static_assert(args: &[Expr], line: usize, out: &mut Vec<CompileDiagnostic>) {
    match args.first() {
        Some(Expr::Bool(false)) => {
            out.push(diag::diag(
                diag::CLPP0701,
                line.max(1),
                1,
                "static_assert condition is false",
                "error",
            ));
        }
        Some(Expr::Number(n)) if n == "0" => {
            out.push(diag::diag(
                diag::CLPP0701,
                line.max(1),
                1,
                "static_assert condition is false",
                "error",
            ));
        }
        Some(Expr::Bool(true)) | Some(Expr::Number(_)) => {}
        Some(_) => {
            out.push(diag::diag(
                diag::CLPP0701,
                line.max(1),
                1,
                "static_assert needs a constant bool or integer",
                "error",
            ));
        }
        None => {
            out.push(diag::diag(
                diag::CLPP0701,
                line.max(1),
                1,
                "static_assert needs a condition",
                "error",
            ));
        }
    }
}

fn check_reflect_call(
    engine: &Engine<'_>,
    name: &str,
    args: &[Expr],
    line: usize,
    out: &mut Vec<CompileDiagnostic>,
) {
    let ty_name = args.first().and_then(|a| match a {
        Expr::Ident(n) => Some(n.as_str()),
        Expr::String(s) => Some(s.as_str()),
        _ => None,
    });
    let Some(ty_name) = ty_name else {
        out.push(CompileDiagnostic {
            message: format!("`{name}` expects a type name"),
            line: line.max(1),
            column: 1,
            severity: "error".into(),
            code: None,
            help: None,
        });
        return;
    };
    if !engine.types.has_struct(ty_name) && engine.symbols.struct_named(ty_name).is_none() {
        out.push(CompileDiagnostic {
            message: format!("comptime `{name}`: unknown type `{ty_name}`"),
            line: line.max(1),
            column: 1,
            severity: "error".into(),
            code: None,
            help: None,
        });
    }
}

fn check_private_named(
    engine: &Engine<'_>,
    owner_name: &str,
    member: &str,
    method_owner: Option<&str>,
    line: usize,
    out: &mut Vec<CompileDiagnostic>,
) {
    if !engine
        .private
        .get(owner_name)
        .is_some_and(|set| set.contains(member))
    {
        return;
    }
    if method_owner == Some(owner_name) {
        return;
    }
    out.push(diag::diag(
        diag::CLPP0401,
        line.max(1),
        1,
        format!("`{member}` is private on `{owner_name}`"),
        "error",
    ));
}

fn check_unknown_ident(
    engine: &Engine<'_>,
    name: &str,
    line: usize,
    method_owner: Option<&str>,
    out: &mut Vec<CompileDiagnostic>,
) {
    let clean = name.trim_start_matches('@');
    if clean == "this" || clean == "self" {
        return;
    }
    if is_bare_global(clean) || builtins::find(clean).is_some() || is_datatype(clean) {
        return;
    }
    // Owner fields/methods without `@` are CLPP0101/0102, not unknown idents.
    if let Some(owner) = resolve_owner_id(engine, line, method_owner) {
        if owner_member_kind(engine, owner, clean).is_some() {
            return;
        }
    }
    if engine.symbols.lookup_at(line, clean).is_some() {
        return;
    }
    if engine.symbols.lookup(engine.symbols.file_scope, clean).is_some() {
        return;
    }
    if engine.types.has_struct(clean) {
        return;
    }
    let candidates: Vec<&str> = engine
        .symbols
        .symbols
        .iter()
        .map(|s| s.name.as_str())
        .collect();
    let hint = diag::did_you_mean(clean, candidates.into_iter());
    let extra = hint
        .map(|h| format!("; did you mean `{h}`?"))
        .unwrap_or_default();
    out.push(CompileDiagnostic {
        message: format!("unknown identifier `{clean}`{extra}"),
        line: line.max(1),
        column: 1,
        severity: "error".into(),
        code: None,
        help: None,
    });
}

fn check_const_assign(
    engine: &Engine<'_>,
    left: &Expr,
    line: usize,
    out: &mut Vec<CompileDiagnostic>,
) {
    let Expr::Ident(name) = left else {
        return;
    };
    let Some(id) = engine.symbols.lookup_at(line, name) else {
        return;
    };
    let Some(sym) = engine.symbols.get(id) else {
        return;
    };
    if sym.is_const {
        out.push(CompileDiagnostic {
            message: format!("cannot assign to const '{name}'"),
            line: line.max(1),
            column: 1,
            severity: "error".into(),
            code: None,
            help: None,
        });
    }
}

fn check_getservice(
    engine: &mut Engine<'_>,
    type_args: &[String],
    args: &[Expr],
    line: usize,
    out: &mut Vec<CompileDiagnostic>,
) {
    let service = type_args
        .first()
        .map(|s| s.as_str())
        .or_else(|| {
            args.first().and_then(|a| match a {
                Expr::String(s) => Some(s.as_str()),
                _ => None,
            })
        });
    let Some(service) = service else {
        return;
    };
    if engine.types.has_struct(service)
        || crate::platform::is_service(service)
        || crate::names::INSTANCE_TYPES.contains(&service)
    {
        return;
    }
    if engine.types.has_struct("Players") {
        out.push(diag::diag(
            diag::CLPP0604,
            line.max(1),
            1,
            format!("unknown service `{service}` for GetService"),
            "warning",
        ));
    }
}

fn check_member_name(
    engine: &mut Engine<'_>,
    object: &Expr,
    name: &str,
    line: usize,
    method_owner: Option<&str>,
    out: &mut Vec<CompileDiagnostic>,
    is_call: bool,
) {
    let obj_ty = engine.type_of_expr(object, line);
    if let Some(info) = engine.types.struct_of(obj_ty) {
        let owner_name = info.name.clone();
        check_private_named(engine, &owner_name, name, method_owner, line, out);
    }
    if engine.types.is_generic_param(obj_ty) {
        let label = engine.types.label(obj_ty);
        if engine.types.generic_bound(obj_ty).is_none() {
            out.push(diag::diag(
                diag::CLPP0901,
                line.max(1),
                1,
                format!("type parameter `{label}` has no bound; cannot access `{name}`"),
                "error",
            ));
            return;
        }
        let members = engine.types.get_members(obj_ty);
        if members.iter().any(|m| m.name == name) {
            return;
        }
        out.push(diag::diag(
            diag::CLPP0901,
            line.max(1),
            1,
            format!("`{name}` is not guaranteed by the bound on `{label}`"),
            "error",
        ));
        return;
    }
    let members = engine.types.get_members(obj_ty);
    if engine.types.is_unknown(obj_ty) || members.is_empty() {
        return;
    }
    if members.iter().any(|m| m.name == name) {
        return;
    }
    if is_call && crate::names::is_method(name) {
        return;
    }
    if engine.types.is_instance_like(obj_ty) {
        return;
    }
    out.push(diag::diag(
        diag::CLPP0604,
        line.max(1),
        1,
        format!(
            "unknown member `{name}` on {}",
            engine.types.label(obj_ty)
        ),
        "error",
    ));
}

fn resolve_call_sig(
    engine: &mut Engine<'_>,
    object: Option<&Expr>,
    name: &str,
    line: usize,
) -> Option<CallSig> {
    if let Some(object) = object {
        let obj_ty = engine.type_of_expr(object, line);
        if let Some(member) = engine.types.lookup_member(obj_ty, name) {
            let owner_name = engine
                .types
                .struct_of(obj_ty)
                .map(|s| s.name.clone())
                .unwrap_or_else(|| engine.types.label(obj_ty));
            let min_from_ast =
                min_params_from_ast(engine.program, Some(&owner_name), name);
            // Synthetic members (signal Connect, array push, …) have empty params —
            // skip arity until the API DB fills them.
            if member.params.is_empty() && min_from_ast.is_none() {
                return None;
            }
            if matches!(
                member.kind,
                MemberKind::Method | MemberKind::Constructor
            ) || !member.params.is_empty()
            {
                let min = min_from_ast.unwrap_or_else(|| {
                    member
                        .params
                        .iter()
                        .take_while(|(_, t)| !engine.types.is_optional(*t))
                        .count()
                });
                return Some(CallSig {
                    label: format!("{owner_name}.{name}"),
                    params: member.params.clone(),
                    min_params: min,
                    ret: member.type_id,
                });
            }
        }
        return None;
    }
    if builtins::find(name).is_some() {
        return None; // arity handled separately
    }
    // Free function from program AST (authoritative for defaults).
    if let Some(func) = find_free_function(engine.program, name) {
        let params: Vec<(String, TypeId)> = func
            .params
            .iter()
            .map(|p| {
                let ty = resolve_func_param_ty(engine, func, p.value_type.as_deref());
                (p.name.clone(), ty)
            })
            .collect();
        let min_params = func
            .params
            .iter()
            .take_while(|p| p.default.is_none())
            .count();
        let ret = func
            .return_type
            .as_deref()
            .filter(|t| !t.is_empty() && *t != "void")
            .map(|t| resolve_func_param_ty(engine, func, Some(t)))
            .unwrap_or(engine.types.void);
        return Some(CallSig {
            label: name.into(),
            params,
            min_params,
            ret,
        });
    }
    // Symbol FunctionType fallback.
    if let Some(id) = engine.symbols.lookup_at(line, name)
        .or_else(|| engine.symbols.lookup(engine.symbols.file_scope, name))
    {
        if let Some(sym) = engine.symbols.get(id) {
            if let Some(ft) = engine.types.as_function(sym.type_id) {
                let params: Vec<(String, TypeId)> = ft
                    .params
                    .iter()
                    .enumerate()
                    .map(|(i, t)| (format!("_{i}"), *t))
                    .collect();
                let n = params.len();
                return Some(CallSig {
                    label: name.into(),
                    params,
                    min_params: n,
                    ret: ft.ret,
                });
            }
        }
    }
    None
}

fn resolve_ctor_sig(engine: &mut Engine<'_>, class_name: &str) -> Option<CallSig> {
    let ty = engine.types.nominal(class_name);
    let member = engine.types.lookup_member(ty, class_name)?;
    if member.kind != MemberKind::Constructor && member.params.is_empty() {
        return None;
    }
    let param_len = member.params.len();
    let min = min_params_from_ast(engine.program, Some(class_name), class_name)
        .unwrap_or(0)
        .min(param_len);
    Some(CallSig {
        label: format!("new {class_name}"),
        params: member.params,
        min_params: min,
        ret: member.type_id,
    })
}

fn find_free_function<'a>(program: &'a Program, name: &str) -> Option<&'a Function> {
    program.items.iter().find_map(|item| match item {
        Item::Function(f) | Item::Proto(f) if f.owner.is_none() && f.name == name => Some(f),
        _ => None,
    })
}

fn resolve_func_param_ty(engine: &mut Engine<'_>, func: &Function, raw: Option<&str>) -> TypeId {
    let Some(raw) = raw else {
        return engine.types.any;
    };
    if func.type_params.iter().any(|tp| tp.name == raw) {
        if let Some(id) = engine
            .symbols
            .lookup_at(func.line.max(1), raw)
            .or_else(|| engine.symbols.lookup(engine.symbols.file_scope, raw))
        {
            if let Some(sym) = engine.symbols.get(id) {
                if sym.kind == crate::symbols::SymbolKind::TypeParam
                    && sym.type_id != TypeId::UNKNOWN
                {
                    return sym.type_id;
                }
            }
        }
        let bound = func
            .type_params
            .iter()
            .find(|tp| tp.name == raw)
            .and_then(|tp| tp.bound.as_deref())
            .map(|b| engine.types.nominal(b));
        return engine.types.fresh_generic(raw, bound);
    }
    parse_type_str(engine.types, raw)
}

fn check_generic_call_bounds(
    engine: &mut Engine<'_>,
    func: &Function,
    type_args: &[String],
    args: &[Expr],
    line: usize,
    out: &mut Vec<CompileDiagnostic>,
) {
    for (i, tp) in func.type_params.iter().enumerate() {
        let Some(bound_name) = &tp.bound else {
            continue;
        };
        let bound_ty = engine.types.nominal(bound_name);
        let concrete = if let Some(targ) = type_args.get(i) {
            Some(parse_type_str(engine.types, targ))
        } else {
            func.params.iter().zip(args.iter()).find_map(|(p, a)| {
                if p.value_type.as_deref() == Some(tp.name.as_str()) {
                    Some(engine.type_of_expr(a, line))
                } else {
                    None
                }
            })
        };
        let Some(concrete) = concrete else {
            continue;
        };
        if engine.types.is_unknown(concrete) {
            continue;
        }
        if !engine.types.satisfies_bound(concrete, bound_ty) {
            out.push(diag::diag(
                diag::CLPP0901,
                line.max(1),
                1,
                format!(
                    "`{}` does not satisfy bound `{bound_name}` for type parameter `{}`",
                    engine.types.label(concrete),
                    tp.name
                ),
                "error",
            ));
        }
    }
}

fn min_params_from_ast(program: &Program, owner: Option<&str>, name: &str) -> Option<usize> {
    for item in &program.items {
        match item {
            Item::Function(f) | Item::Proto(f)
                if f.name == name && f.owner.as_deref() == owner =>
            {
                return Some(
                    f.params
                        .iter()
                        .take_while(|p| p.default.is_none())
                        .count(),
                );
            }
            _ => {}
        }
    }
    None
}

fn check_call_args(
    engine: &mut Engine<'_>,
    sig: &CallSig,
    args: &[Expr],
    line: usize,
    out: &mut Vec<CompileDiagnostic>,
) {
    let max = sig.params.len();
    if args.len() < sig.min_params || args.len() > max {
        out.push(CompileDiagnostic {
            message: format!(
                "`{}` takes {} argument(s), got {}",
                sig.label,
                if sig.min_params == max {
                    format!("{max}")
                } else {
                    format!("{}..{}", sig.min_params, max)
                },
                args.len()
            ),
            line: line.max(1),
            column: 1,
            severity: "error".into(),
            code: None,
            help: None,
        });
    }
    for (i, arg) in args.iter().enumerate() {
        let Some((_, expected)) = sig.params.get(i) else {
            break;
        };
        let actual = engine.type_of_expr(arg, line);
        push_arg_assign(out, line, *expected, actual, engine.types, &sig.label, i);
    }
}

fn push_arg_assign(
    out: &mut Vec<CompileDiagnostic>,
    line: usize,
    expected: TypeId,
    actual: TypeId,
    types: &TypeDatabase,
    label: &str,
    index: usize,
) {
    if types.is_unknown(expected) || types.is_unknown(actual) {
        return;
    }
    if types.is_optional(actual) && !types.is_optional(expected) && !types.is_nil(expected) {
        out.push(diag::diag(
            diag::CLPP0201,
            line.max(1),
            1,
            format!(
                "argument {} of `{label}`: cannot pass optional {} where {} expected",
                index + 1,
                types.label(actual),
                types.label(expected)
            ),
            "error",
        ));
        return;
    }
    // Soft: only flag clear primitive / void mismatches.
    if !soft_mismatch(types, actual, expected) {
        return;
    }
    out.push(CompileDiagnostic {
        message: format!(
            "argument {} of `{label}`: expected {}, got {}",
            index + 1,
            types.label(expected),
            types.label(actual)
        ),
        line: line.max(1),
        column: 1,
        severity: "error".into(),
        code: None,
        help: None,
    });
}

fn soft_mismatch(types: &TypeDatabase, actual: TypeId, expected: TypeId) -> bool {
    if types.is_assignable(actual, expected) {
        return false;
    }
    use crate::types::TypeKind;
    matches!(
        (types.peel(actual), types.peel(expected)),
        (TypeKind::Primitive(a), TypeKind::Primitive(b)) if a != b
    ) || matches!(
        (types.peel(actual), types.peel(expected)),
        (TypeKind::Primitive(_), TypeKind::Nominal(_))
            | (TypeKind::Nominal(_), TypeKind::Primitive(_))
            | (TypeKind::Void, _)
            | (_, TypeKind::Void)
    )
}

enum AssignSite<'a> {
    /// `T name = expr;` / `const T name = expr;`
    Init { name: &'a str, is_const: bool },
    /// `name = expr;` or return
    Flow,
}

fn push_assign(
    out: &mut Vec<CompileDiagnostic>,
    line: usize,
    expected: TypeId,
    actual: TypeId,
    types: &TypeDatabase,
    site: AssignSite<'_>,
) {
    if types.is_unknown(expected) || types.is_unknown(actual) {
        return;
    }
    use crate::types::{Primitive, TypeKind};

    // float → int: warn (Luau does not truncate), do not error.
    if matches!(types.peel(expected), TypeKind::Primitive(Primitive::Int))
        && matches!(types.peel(actual), TypeKind::Primitive(Primitive::Float))
    {
        let who = match site {
            AssignSite::Init { name, .. } => format!("'{name}'"),
            AssignSite::Flow => "target".into(),
        };
        out.push(CompileDiagnostic {
            message: format!(
                "initializing int {who} from float; Luau does not truncate"
            ),
            line: line.max(1),
            column: 1,
            severity: "warning".into(),
            code: None,
            help: None,
        });
        return;
    }

    if types.is_optional(actual) && !types.is_optional(expected) && !types.is_nil(expected) {
        let detail = match site {
            AssignSite::Init { name, is_const } => {
                let prefix = if is_const { "const " } else { "" };
                format!(
                    "cannot initialize '{prefix}{} {name}' with {}",
                    types.label(expected),
                    types.label(actual)
                )
            }
            AssignSite::Flow => format!(
                "cannot assign optional {} to non-optional {}",
                types.label(actual),
                types.label(expected)
            ),
        };
        out.push(diag::diag(
            diag::CLPP0201,
            line.max(1),
            1,
            detail,
            "error",
        ));
        return;
    }

    if types.is_assignable(actual, expected) {
        return;
    }
    if !soft_mismatch(types, actual, expected) {
        return;
    }
    let message = match site {
        AssignSite::Init { name, is_const } => {
            let prefix = if is_const { "const " } else { "" };
            format!(
                "cannot initialize '{prefix}{} {name}' with {}; expected {}",
                types.label(expected),
                types.label(actual),
                types.label(expected)
            )
        }
        AssignSite::Flow => format!(
            "cannot assign {} to {}; expected {}",
            types.label(actual),
            types.label(expected),
            types.label(expected)
        ),
    };
    out.push(CompileDiagnostic {
        message,
        line: line.max(1),
        column: 1,
        severity: "error".into(),
        code: None,
        help: None,
    });
}
