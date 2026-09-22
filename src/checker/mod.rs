//! Type resolver, checker queries, flow-sensitive narrowing, and diagnostic pass.

mod pass;
mod typed;

pub use pass::{check_program, check_program_ex};
pub use typed::check_typed;

/// Name-table pass (`pass`) + TypeId validation (`typed`). Compile/session run both.

use crate::ast::{Expr, Function, Item, Program, Stmt};
use crate::binder::BoundFile;
use crate::symbols::{SymbolDatabase, SymbolId, SymbolKind};
use crate::types::{parse_type_str, MemberKind, StructMember, TypeDatabase, TypeId};

pub fn resolve(program: &Program, bound: &mut BoundFile, types: &mut TypeDatabase) {
    // 1. Register structs / enums / aliases by name.
    for item in &program.items {
        match item {
            Item::Class { name, type_params, .. } => {
                types.define_struct(name, Vec::new());
                if let Some(info) = types.struct_mut(name) {
                    info.type_params = type_params.iter().map(|p| p.name.clone()).collect();
                }
            }
            Item::Enum { name, .. } => {
                types.define_struct(name, Vec::new());
            }
            Item::TypeAlias { name, ty, .. } => {
                let target = parse_type_str(types, ty);
                types.define_alias(name, target);
            }
            _ => {}
        }
    }
    // 2. Bases.
    for item in &program.items {
        if let Item::Class {
            name, parent: Some(parent), ..
        } = item
        {
            let base = types.nominal(parent);
            types.set_bases(name, vec![base]);
            if let Some(sym) = bound.symbols.struct_named(name) {
                if let Some(parent_sym) = bound.symbols.struct_named(parent) {
                    if let Some(s) = bound.symbols.get_mut(sym) {
                        s.parent = Some(parent_sym);
                    }
                }
            }
        }
    }
    // 3. Fill symbol type_ids from declared strings.
    let ids: Vec<SymbolId> = bound
        .symbols
        .symbols
        .iter()
        .map(|s| s.id)
        .collect();
    for id in ids {
        let (declared, kind, name, owner) = {
            let Some(sym) = bound.symbols.get(id) else { continue };
            (
                sym.declared_type.clone(),
                sym.kind,
                sym.name.clone(),
                sym.owner,
            )
        };
        let ty = match kind {
            SymbolKind::Struct | SymbolKind::Enum => types.nominal(short_name(&name)),
            SymbolKind::TypeAlias => declared
                .as_deref()
                .map(|d| parse_type_str(types, d))
                .unwrap_or(types.unknown),
            SymbolKind::EnumVariant => owner
                .and_then(|o| bound.symbols.get(o).map(|s| types.nominal(&s.name)))
                .unwrap_or(types.unknown),
            SymbolKind::TypeParam => {
                let bound_ty = declared.as_deref().map(|b| types.nominal(b));
                types.fresh_generic(short_name(&name), bound_ty)
            }
            _ => {
                let scope = bound
                    .symbols
                    .get(id)
                    .map(|s| s.scope)
                    .unwrap_or(bound.symbols.file_scope);
                if let Some(d) = declared.as_deref() {
                    if let Some(tp_sid) = bound.symbols.lookup(scope, d) {
                        if tp_sid != id {
                            if let Some(tp) = bound.symbols.get(tp_sid) {
                                if tp.kind == SymbolKind::TypeParam
                                    && tp.type_id != TypeId::UNKNOWN
                                {
                                    tp.type_id
                                } else {
                                    parse_type_str(types, d)
                                }
                            } else {
                                parse_type_str(types, d)
                            }
                        } else {
                            parse_type_str(types, d)
                        }
                    } else {
                        parse_type_str(types, d)
                    }
                } else {
                    types.unknown
                }
            }
        };
        if let Some(sym) = bound.symbols.get_mut(id) {
            if ty != TypeId::UNKNOWN || sym.type_id == TypeId::UNKNOWN {
                sym.type_id = ty;
            }
        }
    }
    // 4. Populate StructInfo members from symbols.
    for sym in &bound.symbols.symbols {
        let Some(owner) = sym.owner else { continue };
        let Some(owner_sym) = bound.symbols.get(owner) else { continue };
        if owner_sym.kind != SymbolKind::Struct && owner_sym.kind != SymbolKind::Enum {
            continue;
        }
        let member_kind = match sym.kind {
            SymbolKind::Field | SymbolKind::Property => MemberKind::Field,
            SymbolKind::Constructor => MemberKind::Constructor,
            SymbolKind::Method | SymbolKind::Function => MemberKind::Method,
            _ => continue,
        };
        let short = short_name(&sym.name).to_string();
        let owner_name = owner_sym.name.clone();
        let params = method_params(&bound.symbols, program, &sym.name, &owner_name, types);
        types.add_member(
            &owner_name,
            StructMember {
                name: short,
                type_id: sym.type_id,
                kind: member_kind,
                params,
                is_static: sym.is_static,
                doc: sym.doc.clone(),
            },
        );
    }
    seed_constructors(program, types);
    // 5. Free functions get a Function TypeId (params + return).
    for item in &program.items {
        let (Item::Function(func) | Item::Proto(func)) = item else {
            continue;
        };
        if func.owner.is_some() {
            continue;
        }
        let params: Vec<TypeId> = func
            .params
            .iter()
            .map(|p| {
                p.value_type
                    .as_deref()
                    .map(|t| parse_type_str(types, t))
                    .unwrap_or(types.any)
            })
            .collect();
        let ret = func
            .return_type
            .as_deref()
            .filter(|t| !t.is_empty() && *t != "void")
            .map(|t| parse_type_str(types, t))
            .unwrap_or(types.void);
        let ft = types.function(params, ret);
        if let Some(id) = bound.symbols.lookup(bound.symbols.file_scope, &func.name) {
            if let Some(sym) = bound.symbols.get_mut(id) {
                sym.type_id = ft;
            }
        }
    }
}

fn short_name(name: &str) -> &str {
    name.rsplit("::").next().unwrap_or(name)
}

fn method_params(
    _symbols: &SymbolDatabase,
    program: &Program,
    full_name: &str,
    owner: &str,
    types: &mut TypeDatabase,
) -> Vec<(String, TypeId)> {
    let short = short_name(full_name);
    for item in &program.items {
        match item {
            Item::Function(func) | Item::Proto(func) => {
                if func.name == short && func.owner.as_deref() == Some(owner) {
                    return func
                        .params
                        .iter()
                        .map(|p| {
                            let ty = p
                                .value_type
                                .as_deref()
                                .map(|t| parse_type_str(types, t))
                                .unwrap_or(types.unknown);
                            (p.name.clone(), ty)
                        })
                        .collect();
                }
            }
            _ => {}
        }
    }
    Vec::new()
}

fn seed_constructors(program: &Program, types: &mut TypeDatabase) {
    for item in &program.items {
        if let Item::Class { name, .. } = item {
            if types
                .struct_info(name)
                .map(|s| s.members.iter().any(|m| m.kind == MemberKind::Constructor))
                .unwrap_or(false)
            {
                continue;
            }
            let instance = types.nominal("Instance");
            let parent = types.optional(instance);
            let ret = types.nominal(name);
            types.add_member(
                name,
                StructMember {
                    name: name.clone(),
                    type_id: ret,
                    kind: MemberKind::Constructor,
                    params: vec![("parent".into(), parent)],
                    is_static: true,
                    doc: Some("new".into()),
                },
            );
        }
    }
}

/// Query engine over a bound+resolved file.
pub struct Engine<'a> {
    pub program: &'a Program,
    pub symbols: &'a SymbolDatabase,
    pub types: &'a mut TypeDatabase,
    pub source: &'a str,
    /// `owner -> private member names` (CLPP0401).
    pub private: std::collections::HashMap<String, std::collections::HashSet<String>>,
}

impl<'a> Engine<'a> {
    pub fn type_of_name(&mut self, line: usize, name: &str) -> TypeId {
        let clean = name.trim_start_matches('@');
        if clean == "this" || clean == "self" {
            if let Some(owner) = self.symbols.enclosing_owner(line) {
                if let Some(sym) = self.symbols.get(owner) {
                    return self.types.nominal(&sym.name);
                }
            }
        }
        if let Some(id) = self.symbols.lookup_at(line, clean) {
            if let Some(sym) = self.symbols.get(id) {
                return self.types.peel_id(sym.type_id);
            }
        }
        if let Some(id) = self.symbols.lookup(self.symbols.file_scope, clean) {
            if let Some(sym) = self.symbols.get(id) {
                return self.types.peel_id(sym.type_id);
            }
        }
        self.types.nominal(clean)
    }

    pub fn type_of_expr(&mut self, expr: &Expr, line: usize) -> TypeId {
        match expr {
            Expr::Null => self.types.nil,
            Expr::Bool(_) => self.types.boolean,
            Expr::Number(n) => {
                if n.contains('.') {
                    self.types.float
                } else {
                    self.types.int
                }
            }
            Expr::String(_) | Expr::Interp { .. } => self.types.string,
            Expr::Ident(name) => self.type_of_name(line, name),
            Expr::This { line: l } => self.type_of_name(*l, "self"),
            Expr::AtField { name, line: l } => {
                let recv = self.type_of_name(*l, "self");
                self.member_type(recv, name)
            }
            Expr::New { class_name, .. } => self.types.nominal(class_name),
            Expr::Cast { value_type, .. } => parse_type_str(self.types, value_type),
            Expr::Member { object, name, .. } => {
                let obj = self.type_of_expr(object, line);
                self.member_type(obj, name)
            }
            Expr::Call {
                object,
                name,
                access,
                args,
                type_args,
                ..
            } => {
                if name == "GetService" {
                    if let Some(targ) = type_args.first() {
                        return self.types.nominal(targ);
                    }
                    if let Some(Expr::String(s)) = args.first() {
                        return self.types.nominal(s);
                    }
                }
                if let Some(targ) = type_args.first() {
                    if name == "FindFirstChild"
                        || name == "FindFirstChildOfClass"
                        || name == "FindFirstChildWhichIsA"
                    {
                        let inner = parse_type_str(self.types, targ);
                        return self.types.optional(inner);
                    }
                    if name == "WaitForChild" {
                        return parse_type_str(self.types, targ);
                    }
                }
                if let Some(object) = object {
                    let obj_ty = self.type_of_expr(object, line);
                    if crate::ast::AccessKind::parse(access).is_janitor()
                        || name == "Connect"
                        || name == "Once"
                        || name == "Wait"
                    {
                        if self.types.is_signal(obj_ty) {
                            if name == "Wait" {
                                if let Some(params) = self.types.signal_params(obj_ty) {
                                    return params.first().copied().unwrap_or(self.types.unknown);
                                }
                            }
                            return self.types.nominal("RBXScriptConnection");
                        }
                    }
                    if name == "IsA" {
                        return self.types.boolean;
                    }
                    return self.member_type(obj_ty, name);
                }
                if let Some(id) = self.symbols.lookup_at(line, name) {
                    if let Some(sym) = self.symbols.get(id) {
                        return sym.type_id;
                    }
                }
                self.types.unknown
            }
            Expr::OptionalChain { object, name, args } => {
                let obj = self.type_of_expr(object, line);
                let inner = self.types.unwrap_optional(obj).unwrap_or(obj);
                let ty = if args.is_some() {
                    self.member_type(inner, name)
                } else {
                    self.member_type(inner, name)
                };
                self.types.optional(ty)
            }
            Expr::Unary { argument, .. } | Expr::Await { argument } | Expr::Update { target: argument, .. } => {
                self.type_of_expr(argument, line)
            }
            Expr::Binary { op, left, right } => {
                if op == ".:" {
                    return self.types.string;
                }
                let l = self.type_of_expr(left, line);
                let r = self.type_of_expr(right, line);
                match op.as_str() {
                    "==" | "!=" | "<" | ">" | "<=" | ">=" | "&&" | "||" => self.types.boolean,
                    "+" | "-" | "*" | "/" | "%" | "**" => {
                        if l == self.types.float || r == self.types.float {
                            self.types.float
                        } else {
                            l
                        }
                    }
                    _ => l,
                }
            }
            Expr::Assign { right, .. } => self.type_of_expr(right, line),
            Expr::Lambda { params, .. } => {
                let ps: Vec<TypeId> = params
                    .iter()
                    .map(|p| {
                        p.value_type
                            .as_deref()
                            .map(|t| parse_type_str(self.types, t))
                            .unwrap_or(self.types.any)
                    })
                    .collect();
                let ret = self.types.any;
                self.types.function(ps, ret)
            }
            Expr::ArrayLit { elements } => {
                let elem = elements
                    .first()
                    .map(|e| self.type_of_expr(e, line))
                    .unwrap_or(self.types.any);
                self.types.array(elem)
            }
            Expr::Index { object, .. } => {
                let obj = self.type_of_expr(object, line);
                self.types.array_elem(obj).unwrap_or(self.types.unknown)
            }
            Expr::Coalesce { left, right } => {
                let l = self.type_of_expr(left, line);
                let r = self.type_of_expr(right, line);
                self.types.unwrap_optional(l).unwrap_or(l).max_with(r)
            }
            Expr::Ternary {
                then_expr,
                else_expr,
                ..
            } => {
                let a = self.type_of_expr(then_expr, line);
                let b = self.type_of_expr(else_expr, line);
                if a == b {
                    a
                } else {
                    self.types.union_of(vec![a, b])
                }
            }
            Expr::InitList { .. } | Expr::DictLit { .. } | Expr::Tuple(_) => self.types.unknown,
        }
    }

    pub fn member_type(&mut self, obj: TypeId, name: &str) -> TypeId {
        let obj = self.types.unwrap_optional(obj).unwrap_or(obj);
        if let Some(member) = self.types.lookup_member(obj, name) {
            return member.type_id;
        }
        if self.types.is_signal(obj) {
            return match name {
                "Connect" | "Once" => self.types.nominal("RBXScriptConnection"),
                "Wait" => self
                    .types
                    .signal_params(obj)
                    .and_then(|p| p.first().copied())
                    .unwrap_or(self.types.unknown),
                "Fire" => self.types.void,
                _ => self.types.unknown,
            };
        }
        self.types.unknown
    }

    pub fn get_members(&self, id: TypeId) -> Vec<StructMember> {
        self.types.get_members(id)
    }

    /// Apply `guard` / `if (optional)` facts at `line` inside a function.
    pub fn type_of_name_flow(&mut self, line: usize, name: &str) -> TypeId {
        let mut ty = self.type_of_name(line, name);
        if let Some(func) = enclosing_function(self.program, line) {
            ty = apply_flow(self.types, &func.body, line, name, ty);
        }
        if !self.source.is_empty() {
            ty = narrow_from_source(self.source, line, name, self.types, ty);
        }
        ty
    }
}

/// Source-based narrowing for `guard` / `if (optional)` when AST spans are incomplete.
pub fn narrow_from_source(
    source: &str,
    line: usize,
    name: &str,
    types: &mut TypeDatabase,
    mut ty: TypeId,
) -> TypeId {
    let lines: Vec<&str> = source.lines().collect();
    let mut guard_else_depth: i32 = 0;
    let mut saw_exiting_guard = false;
    let mut if_then_depth: i32 = 0;
    let mut in_narrowing_if = false;
    let mut isa_class: Option<String> = None;
    for (i, raw) in lines.iter().enumerate() {
        let lno = i + 1;
        if lno > line {
            break;
        }
        let t = raw.trim();
        let guard_hit = t.contains(&format!("guard ({name}"))
            || t.contains(&format!("guard({name}"));
        if guard_hit {
            saw_exiting_guard = true;
            guard_else_depth = 1;
        }
        let if_hit = t.contains(&format!("if ({name}"))
            || t.contains(&format!("if({name}"));
        if if_hit {
            in_narrowing_if = true;
            if_then_depth = 1;
        }
        if let Some(class) = parse_isa_line(t, name) {
            isa_class = Some(class);
            in_narrowing_if = true;
            if_then_depth = 1;
        }
        let opens = t.matches('{').count() as i32;
        let closes = t.matches('}').count() as i32;
        if guard_else_depth > 0 {
            guard_else_depth += opens - closes;
            if guard_else_depth <= 0 {
                guard_else_depth = 0;
            }
        }
        if if_then_depth > 0 {
            if_then_depth += opens - closes;
            if if_then_depth <= 0 {
                in_narrowing_if = false;
                if_then_depth = 0;
                isa_class = None;
            }
        }
    }
    let in_guard_else = guard_else_depth > 0;
    if saw_exiting_guard && !in_guard_else {
        ty = types.exclude_nil(ty);
    }
    if in_narrowing_if {
        ty = types.exclude_nil(ty);
        if let Some(class) = isa_class {
            ty = types.nominal(&class);
        }
    }
    ty
}

fn parse_isa_line(t: &str, name: &str) -> Option<String> {
    let needle = format!("{name}.IsA(\"");
    let idx = t.find(&needle)?;
    let rest = &t[idx + needle.len()..];
    let end = rest.find('"')?;
    Some(rest[..end].to_string())
}

trait DummyMax {
    fn max_with(self, other: TypeId) -> TypeId;
}

impl DummyMax for TypeId {
    fn max_with(self, other: TypeId) -> TypeId {
        if self == TypeId::UNKNOWN {
            other
        } else {
            self
        }
    }
}

fn enclosing_function(program: &Program, line: usize) -> Option<&Function> {
    program.items.iter().find_map(|item| match item {
        Item::Function(func)
            if func.line <= line && (func.span.end_line == 0 || line <= func.span.end_line) =>
        {
            Some(func)
        }
        _ => None,
    })
}

fn apply_flow(
    types: &mut TypeDatabase,
    stmts: &[Stmt],
    line: usize,
    name: &str,
    mut ty: TypeId,
) -> TypeId {
    walk_flow(types, stmts, line, name, &mut ty);
    ty
}

fn walk_flow(
    types: &mut TypeDatabase,
    stmts: &[Stmt],
    line: usize,
    name: &str,
    ty: &mut TypeId,
) -> bool {
    for stmt in stmts {
        match stmt {
            Stmt::Guard { test, body } => {
                let in_else = body.iter().any(|s| s.span().contains_line(line) || s.line() == line);
                if in_else {
                    walk_flow(types, body, line, name, ty);
                    return true;
                }
                if guard_narrows(test, name) {
                    *ty = types.exclude_nil(*ty);
                }
            }
            Stmt::If {
                test,
                consequent,
                alternate,
            } => {
                let in_then = consequent.iter().any(|s| {
                    let sp = s.span();
                    sp.contains_line(line) || s.line() == line
                });
                if in_then {
                    if truthy_narrows(test, name) {
                        *ty = types.exclude_nil(*ty);
                    }
                    if let Some(class) = isa_narrow(test, name) {
                        *ty = types.nominal(&class);
                    }
                    walk_flow(types, consequent, line, name, ty);
                    return true;
                }
                if let Some(alt) = alternate {
                    let in_else = alt.iter().any(|s| s.span().contains_line(line) || s.line() == line);
                    if in_else {
                        walk_flow(types, alt, line, name, ty);
                        return true;
                    }
                }
                if truthy_narrows(test, name) && stmt_exits(consequent) {
                    // then returns; remaining code keeps original (still optional)
                }
            }
            Stmt::Decl(decl) if decl.name == name => {
                if let Some(t) = &decl.value_type {
                    *ty = parse_type_str(types, t);
                }
            }
            other => {
                let nested: Option<&[Stmt]> = match other {
                    Stmt::While { body, .. }
                    | Stmt::DoWhile { body, .. }
                    | Stmt::Spawn { body, .. }
                    | Stmt::Block(body)
                    | Stmt::Comptime { body, .. }
                    | Stmt::Delay { body, .. }
                    | Stmt::Defer { body } => Some(body),
                    Stmt::ForEach { body, .. } | Stmt::CFor { body, .. } => Some(body),
                    _ => None,
                };
                if let Some(body) = nested {
                    if walk_flow(types, body, line, name, ty) {
                        return true;
                    }
                }
            }
        }
    }
    false
}

fn stmt_exits(stmts: &[Stmt]) -> bool {
    stmts.iter().any(|s| matches!(s, Stmt::Return(_) | Stmt::Break | Stmt::Continue))
}

fn guard_narrows(test: &Expr, name: &str) -> bool {
    match test {
        Expr::Ident(n) => n == name,
        Expr::Binary {
            op,
            left,
            right,
        } if op == "!=" => {
            (matches!(left.as_ref(), Expr::Ident(n) if n == name)
                && matches!(right.as_ref(), Expr::Null))
                || (matches!(right.as_ref(), Expr::Ident(n) if n == name)
                    && matches!(left.as_ref(), Expr::Null))
        }
        _ => false,
    }
}

fn truthy_narrows(test: &Expr, name: &str) -> bool {
    guard_narrows(test, name) || matches!(test, Expr::Ident(n) if n == name)
}

fn isa_narrow(test: &Expr, name: &str) -> Option<String> {
    match test {
        Expr::Call {
            object: Some(obj),
            name: method,
            args,
            ..
        } if method == "IsA" => {
            let ident = match obj.as_ref() {
                Expr::Ident(n) => n.as_str(),
                Expr::Member { .. } => return None,
                _ => return None,
            };
            if ident != name {
                return None;
            }
            if let Some(Expr::String(class)) = args.first() {
                return Some(class.clone());
            }
        }
        _ => {}
    }
    None
}

/// Completion / hover helper: type the expression to the left of `.` / `~>` / `::`.
pub fn type_of_prefix(
    engine: &mut Engine,
    line: usize,
    prefix: &str,
) -> TypeId {
    let trimmed = prefix.trim_end();
    let object = strip_trailing_access(trimmed);
    if object.is_empty() {
        return engine.types.unknown;
    }
    type_of_chain(engine, line, object)
}

fn strip_trailing_access(s: &str) -> &str {
    let s = s.trim_end();
    if let Some(rest) = s.strip_suffix("~>") {
        return rest.trim_end();
    }
    if let Some(rest) = s.strip_suffix("::") {
        return rest.trim_end();
    }
    if let Some(rest) = s.strip_suffix('.') {
        if !s.ends_with(".:") {
            return rest.trim_end();
        }
    }
    s
}

fn type_of_chain(engine: &mut Engine, line: usize, expr: &str) -> TypeId {
    let expr = expr.trim();
    if expr.is_empty() {
        return engine.types.unknown;
    }
    if let Some(idx) = expr.rfind(" as ") {
        let ty = expr[idx + 4..].trim();
        let ty = ty.split('(').next().unwrap_or(ty).trim();
        return parse_type_str(engine.types, ty);
    }
    if let Some(inner) = expr.strip_prefix("static_cast<") {
        if let Some((ty, _)) = inner.split_once('>') {
            return parse_type_str(engine.types, ty.trim());
        }
    }
    if let Some(inner) = expr.strip_prefix("GetService<") {
        if let Some((ty, _)) = inner.split_once('>') {
            return engine.types.nominal(ty.trim());
        }
    }
    if let Some(rest) = expr.strip_prefix("new ") {
        let name = rest
            .split('(')
            .next()
            .unwrap_or(rest)
            .trim();
        return engine.types.nominal(name);
    }
    if let Some(rest) = expr.strip_prefix('@') {
        let name = ident_start(rest);
        if name == "this" {
            return engine.type_of_name_flow(line, "self");
        }
        let recv = engine.type_of_name_flow(line, "self");
        let rest = rest[name.len()..].trim_start();
        let ty = engine.member_type(recv, name);
        return continue_chain(engine, line, ty, rest);
    }
    // Split last call/member.
    if let Some(idx) = find_last_access(expr) {
        let (left, acc, right) = idx;
        let obj_ty = type_of_chain(engine, line, left);
        let (name, rest) = split_ident(right);
        let (type_args, rest) = take_type_args(rest);
        let mut ty = if let Some(targ) = type_args.first() {
            if matches!(
                name,
                "FindFirstChild" | "FindFirstChildOfClass" | "FindFirstChildWhichIsA"
            ) {
                let inner = parse_type_str(engine.types, targ);
                engine.types.optional(inner)
            } else if name == "WaitForChild" {
                parse_type_str(engine.types, targ)
            } else {
                engine.member_type(obj_ty, name)
            }
        } else if acc == "~>" {
            engine.member_type(obj_ty, name)
        } else {
            engine.member_type(obj_ty, name)
        };
        let rest = rest.trim_start();
        if rest.starts_with('(') {
            let after = skip_parens(rest);
            ty = continue_chain(engine, line, ty, after);
        } else {
            ty = continue_chain(engine, line, ty, rest);
        }
        return ty;
    }
    let (name, rest) = split_ident(expr);
    if name.is_empty() {
        return engine.types.unknown;
    }
    let mut ty = engine.type_of_name_flow(line, name);
    if rest.trim_start().starts_with('(') {
        let after = skip_parens(rest.trim_start());
        ty = continue_chain(engine, line, ty, after);
    } else {
        ty = continue_chain(engine, line, ty, rest);
    }
    ty
}

fn continue_chain(engine: &mut Engine, line: usize, mut ty: TypeId, rest: &str) -> TypeId {
    let mut rest = rest.trim_start();
    while !rest.is_empty() {
        if rest.starts_with("~>") {
            rest = rest[2..].trim_start();
            let (name, more) = split_ident(rest);
            ty = engine.member_type(ty, name);
            rest = more;
            continue;
        }
        if rest.starts_with("::") {
            rest = rest[2..].trim_start();
            let (name, more) = split_ident(rest);
            ty = engine.member_type(ty, name);
            rest = more;
            continue;
        }
        if rest.starts_with('.') && !rest.starts_with(".:") {
            rest = rest[1..].trim_start();
            let (name, more) = split_ident(rest);
            ty = engine.member_type(ty, name);
            rest = more;
            continue;
        }
        if rest.starts_with('(') {
            rest = skip_parens(rest);
            continue;
        }
        if rest.starts_with('<') {
            rest = skip_angles(rest);
            continue;
        }
        break;
    }
    let _ = line;
    ty
}

fn ident_start(s: &str) -> &str {
    let n = s
        .chars()
        .take_while(|c| c.is_ascii_alphanumeric() || *c == '_')
        .count();
    &s[..n]
}

fn split_ident(s: &str) -> (&str, &str) {
    let s = s.trim_start();
    let n = s
        .chars()
        .take_while(|c| c.is_ascii_alphanumeric() || *c == '_')
        .count();
    (&s[..n], &s[n..])
}

fn take_type_args(s: &str) -> (Vec<String>, &str) {
    let s = s.trim_start();
    if !s.starts_with('<') {
        return (Vec::new(), s);
    }
    let mut depth = 0i32;
    for (i, c) in s.char_indices() {
        if c == '<' {
            depth += 1;
        } else if c == '>' {
            depth -= 1;
            if depth == 0 {
                let inner = &s[1..i];
                let args = inner
                    .split(',')
                    .map(|p| p.trim().to_string())
                    .filter(|p| !p.is_empty())
                    .collect();
                return (args, s[i + 1..].trim_start());
            }
        }
    }
    (Vec::new(), s)
}

fn find_last_access(s: &str) -> Option<(&str, &str, &str)> {
    if let Some(idx) = s.rfind("~>") {
        return Some((&s[..idx], "~>", &s[idx + 2..]));
    }
    if let Some(idx) = s.rfind("::") {
        return Some((&s[..idx], "::", &s[idx + 2..]));
    }
    if let Some(idx) = s.rfind('.') {
        if idx > 0 && !s[idx.saturating_sub(1)..].starts_with(".:") {
            return Some((&s[..idx], ".", &s[idx + 1..]));
        }
    }
    None
}

fn skip_parens(s: &str) -> &str {
    skip_balanced(s, '(', ')')
}

fn skip_angles(s: &str) -> &str {
    skip_balanced(s, '<', '>')
}

fn skip_balanced(s: &str, open: char, close: char) -> &str {
    let mut depth = 0;
    for (i, c) in s.char_indices() {
        if c == open {
            depth += 1;
        } else if c == close {
            depth -= 1;
            if depth == 0 {
                return s[i + c.len_utf8()..].trim_start();
            }
        }
    }
    ""
}
