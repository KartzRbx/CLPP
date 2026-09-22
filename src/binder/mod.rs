//! Binder: AST → symbols and nested scopes. Does not type-check.

use crate::ast::{Decl, Expr, Function, Item, Program, Span, Stmt};
use crate::symbols::{ScopeId, ScopeKind, SymbolDatabase, SymbolId, SymbolKind};

#[derive(Clone, Debug)]
pub struct BoundFile {
    pub symbols: SymbolDatabase,
}

pub fn bind(program: &Program) -> BoundFile {
    let mut db = SymbolDatabase::new();
    let file = db.file_scope;
    // Pass 1: structs, aliases, enums, free decls, prototypes.
    for item in &program.items {
        bind_item_decl(&mut db, file, item);
    }
    // Pass 2: function / method bodies with nested scopes.
    for item in &program.items {
        bind_item_body(&mut db, file, item);
    }
    BoundFile { symbols: db }
}

fn bind_item_decl(db: &mut SymbolDatabase, file: ScopeId, item: &Item) {
    match item {
        Item::Class {
            name,
            parent,
            type_params,
            span,
            doc,
            ..
        } => {
            let id = db.alloc(
                name.clone(),
                SymbolKind::Struct,
                parent.clone(),
                *span,
                file,
                None,
            );
            if let Some(sym) = db.get_mut(id) {
                sym.doc = doc.clone();
            }
            let struct_scope = db.alloc_scope(ScopeKind::Struct, file, *span, Some(id));
            for param in type_params {
                db.alloc(
                    param.name.clone(),
                    SymbolKind::TypeParam,
                    param.bound.clone(),
                    *span,
                    struct_scope,
                    Some(id),
                );
            }
            if let Some(parent_name) = parent {
                let _ = parent_name;
                let _ = struct_scope;
            }
        }
        Item::TypeAlias {
            name,
            ty,
            span,
            doc,
            ..
        } => {
            let id = db.alloc(
                name.clone(),
                SymbolKind::TypeAlias,
                Some(ty.clone()),
                *span,
                file,
                None,
            );
            if let Some(sym) = db.get_mut(id) {
                sym.doc = doc.clone();
            }
        }
        Item::Enum {
            name,
            variants,
            span,
            doc,
            ..
        } => {
            let id = db.alloc(name.clone(), SymbolKind::Enum, None, *span, file, None);
            if let Some(sym) = db.get_mut(id) {
                sym.doc = doc.clone();
            }
            for (variant, _) in variants {
                db.alloc(
                    variant.clone(),
                    SymbolKind::EnumVariant,
                    Some(name.clone()),
                    *span,
                    file,
                    Some(id),
                );
            }
        }
        Item::Decl(decl) => bind_decl(db, file, decl),
        Item::Function(func) | Item::Proto(func) => bind_func_sig(db, file, func),
        Item::Destructure { names, .. } => {
            for name in names {
                db.alloc(
                    name.clone(),
                    SymbolKind::Variable,
                    Some("auto".into()),
                    Span::default(),
                    file,
                    None,
                );
            }
        }
        Item::Unsupported { .. } | Item::Import { .. } => {}
    }
}

fn bind_decl(db: &mut SymbolDatabase, scope: ScopeId, decl: &Decl) {
    let owner = decl
        .owner
        .as_ref()
        .and_then(|name| db.struct_named(name));
    let kind = if owner.is_some() {
        SymbolKind::Field
    } else {
        SymbolKind::Variable
    };
    let id = db.alloc(
        decl.name.clone(),
        kind,
        decl.value_type.clone(),
        decl.span,
        if owner.is_some() { db.file_scope } else { scope },
        owner,
    );
    if let Some(sym) = db.get_mut(id) {
        sym.is_const = decl.is_const;
        sym.doc = decl.doc.clone();
    }
    if let Some(owner_id) = owner {
        // Also register the field name on the struct scope if present.
        for sc in db.scopes.iter_mut() {
            if sc.owner == Some(owner_id) && sc.kind == ScopeKind::Struct {
                sc.names.insert(decl.name.clone(), id);
            }
        }
    }
}

fn bind_func_sig(db: &mut SymbolDatabase, file: ScopeId, func: &Function) {
    let owner = func
        .owner
        .as_ref()
        .and_then(|name| db.struct_named(name));
    let kind = if func.name == func.owner.as_deref().unwrap_or("") {
        SymbolKind::Constructor
    } else if owner.is_some() {
        SymbolKind::Method
    } else {
        SymbolKind::Function
    };
    let id = db.alloc(
        if let Some(owner_name) = &func.owner {
            format!("{}::{}", owner_name, func.name)
        } else {
            func.name.clone()
        },
        kind,
        func.return_type.clone(),
        func.span,
        file,
        owner,
    );
    if let Some(sym) = db.get_mut(id) {
        sym.is_const = func.is_const;
        sym.is_static = func.is_static;
        sym.doc = func.doc.clone();
        // Store the short name for member lookup as well.
        let _ = &func.name;
    }
    // Also bind short name in file scope for free functions.
    if owner.is_none() {
        if let Some(sc) = db.scopes.get_mut(file.0 as usize) {
            sc.names.insert(func.name.clone(), id);
        }
    } else if let Some(owner_id) = owner {
        for sc in db.scopes.iter_mut() {
            if sc.owner == Some(owner_id) && sc.kind == ScopeKind::Struct {
                sc.names.insert(func.name.clone(), id);
            }
        }
    }
}

fn bind_item_body(db: &mut SymbolDatabase, file: ScopeId, item: &Item) {
    match item {
        Item::Function(func) => bind_func_body(db, file, func),
        _ => {}
    }
}

fn bind_func_body(db: &mut SymbolDatabase, file: ScopeId, func: &Function) {
    let owner = func
        .owner
        .as_ref()
        .and_then(|name| db.struct_named(name));
    let mut span = if func.span.end_line == 0 {
        Span::new(func.line, 1, func.line.max(1), 1)
    } else {
        func.span
    };
    span = stmts_span(span, &func.body);
    if span.end_line < span.start_line {
        span.end_line = span.start_line;
    }
    // Ensure body lines after the signature are visible to enclosing_owner.
    if span.end_line < func.line {
        span.end_line = func.line;
    }
    let fn_scope = db.alloc_scope(ScopeKind::Function, file, span, owner);
    for param in &func.type_params {
        db.alloc(
            param.name.clone(),
            SymbolKind::TypeParam,
            param.bound.clone(),
            span,
            fn_scope,
            owner,
        );
    }
    if owner.is_some() && !func.is_static {
        let self_id = db.alloc(
            "self".into(),
            SymbolKind::Variable,
            func.owner.clone(),
            span,
            fn_scope,
            owner,
        );
        if let Some(sc) = db.scopes.get_mut(fn_scope.0 as usize) {
            sc.names.insert("@this".into(), self_id);
            sc.names.insert("this".into(), self_id);
        }
    }
    for param in &func.params {
        db.alloc(
            param.name.clone(),
            SymbolKind::Parameter,
            param.value_type.clone(),
            span,
            fn_scope,
            owner,
        );
        if let Some(default) = &param.default {
            bind_expr(db, fn_scope, owner, default);
        }
    }
    bind_stmts(db, fn_scope, owner, &func.body);
}

fn bind_stmts(db: &mut SymbolDatabase, scope: ScopeId, owner: Option<SymbolId>, stmts: &[Stmt]) {
    for stmt in stmts {
        bind_stmt(db, scope, owner, stmt);
    }
}

fn stmts_span(base: Span, stmts: &[Stmt]) -> Span {
    stmts.iter().fold(base, |acc, s| acc.cover(s.span()))
}

fn bind_stmt(db: &mut SymbolDatabase, scope: ScopeId, owner: Option<SymbolId>, stmt: &Stmt) {
    match stmt {
        Stmt::Decl(decl) => {
            let id = db.alloc(
                decl.name.clone(),
                SymbolKind::Variable,
                decl.value_type.clone(),
                decl.span,
                scope,
                None,
            );
            if let Some(sym) = db.get_mut(id) {
                sym.is_const = decl.is_const;
            }
            if let Some(value) = &decl.value {
                bind_expr(db, scope, owner, value);
            }
        }
        Stmt::Destructure { names, value, .. } | Stmt::FieldDestructure { names, value } => {
            bind_expr(db, scope, owner, value);
            for name in names {
                db.alloc(
                    name.clone(),
                    SymbolKind::Variable,
                    Some("auto".into()),
                    stmt.span(),
                    scope,
                    None,
                );
            }
        }
        Stmt::Expr(expr) | Stmt::Return(Some(expr)) => bind_expr(db, scope, owner, expr),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => {
            bind_expr(db, scope, owner, test);
            let then_span = stmts_span(stmt.span(), consequent);
            let then_scope = db.alloc_scope(ScopeKind::Block, scope, then_span, owner);
            bind_stmts(db, then_scope, owner, consequent);
            if let Some(alt) = alternate {
                let else_span = stmts_span(stmt.span(), alt);
                let else_scope = db.alloc_scope(ScopeKind::Block, scope, else_span, owner);
                bind_stmts(db, else_scope, owner, alt);
            }
        }
        Stmt::Guard { test, body } => {
            bind_expr(db, scope, owner, test);
            let gspan = stmts_span(stmt.span(), body);
            let gscope = db.alloc_scope(ScopeKind::Block, scope, gspan, owner);
            bind_stmts(db, gscope, owner, body);
        }
        Stmt::While { test, body } | Stmt::DoWhile { test, body } => {
            bind_expr(db, scope, owner, test);
            let bspan = stmts_span(stmt.span(), body);
            let bscope = db.alloc_scope(ScopeKind::Block, scope, bspan, owner);
            bind_stmts(db, bscope, owner, body);
        }
        Stmt::ForEach {
            name,
            elem_type,
            iter,
            body,
            span,
        } => {
            bind_expr(db, scope, owner, iter);
            let bspan = stmts_span(*span, body);
            let bscope = db.alloc_scope(ScopeKind::Block, scope, bspan, owner);
            db.alloc(
                name.clone(),
                SymbolKind::Variable,
                elem_type.clone(),
                *span,
                bscope,
                None,
            );
            bind_stmts(db, bscope, owner, body);
        }
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => {
            let mut span = stmt.span();
            span = stmts_span(span, body);
            let bscope = db.alloc_scope(ScopeKind::Block, scope, span, owner);
            if let Some(init) = init {
                bind_stmt(db, bscope, owner, init);
            }
            if let Some(test) = test {
                bind_expr(db, bscope, owner, test);
            }
            if let Some(incr) = incr {
                bind_expr(db, bscope, owner, incr);
            }
            bind_stmts(db, bscope, owner, body);
        }
        Stmt::Switch { discriminant, cases } => {
            bind_expr(db, scope, owner, discriminant);
            for case in cases {
                let cspan = stmts_span(stmt.span(), &case.body);
                let cscope = db.alloc_scope(ScopeKind::Block, scope, cspan, owner);
                bind_stmts(db, cscope, owner, &case.body);
            }
        }
        Stmt::Match { discriminant, arms } => {
            bind_expr(db, scope, owner, discriminant);
            for arm in arms {
                let cspan = stmts_span(stmt.span(), &arm.body);
                let cscope = db.alloc_scope(ScopeKind::Block, scope, cspan, owner);
                if let (Some(class), Some(binding)) = (&arm.class_name, &arm.binding) {
                    db.alloc(
                        binding.clone(),
                        SymbolKind::Variable,
                        Some(class.clone()),
                        stmt.span(),
                        cscope,
                        None,
                    );
                }
                bind_stmts(db, cscope, owner, &arm.body);
            }
        }
        Stmt::Spawn { body, .. }
        | Stmt::Delay { body, .. }
        | Stmt::Defer { body }
        | Stmt::Comptime { body, .. }
        | Stmt::Block(body) => {
            let bscope = db.alloc_scope(ScopeKind::Block, scope, stmt.span(), owner);
            bind_stmts(db, bscope, owner, body);
        }
        Stmt::Try {
            body,
            err_name,
            catch,
        } => {
            let tscope = db.alloc_scope(ScopeKind::Block, scope, stmt.span(), owner);
            bind_stmts(db, tscope, owner, body);
            let cscope = db.alloc_scope(ScopeKind::Block, scope, stmt.span(), owner);
            db.alloc(
                err_name.clone(),
                SymbolKind::Variable,
                Some("string".into()),
                stmt.span(),
                cscope,
                None,
            );
            bind_stmts(db, cscope, owner, catch);
        }
        Stmt::Return(None) | Stmt::Break | Stmt::Continue => {}
    }
}

fn bind_expr(db: &mut SymbolDatabase, scope: ScopeId, owner: Option<SymbolId>, expr: &Expr) {
    match expr {
        Expr::AtField { name, line } => {
            // Receiver field: ensure the name is visible via owner members; no new symbol.
            let _ = (db, scope, owner, name, line);
        }
        Expr::This { .. } => {}
        Expr::Unary { argument, .. }
        | Expr::Await { argument }
        | Expr::Cast { argument, .. }
        | Expr::Update { target: argument, .. } => bind_expr(db, scope, owner, argument),
        Expr::Binary { left, right, .. }
        | Expr::Assign { left, right, .. }
        | Expr::Coalesce { left, right }
        | Expr::Index {
            object: left,
            index: right,
        } => {
            bind_expr(db, scope, owner, left);
            bind_expr(db, scope, owner, right);
        }
        Expr::Member { object, .. } | Expr::OptionalChain { object, .. } => {
            bind_expr(db, scope, owner, object);
        }
        Expr::Call { object, args, .. } => {
            if let Some(object) = object {
                bind_expr(db, scope, owner, object);
            }
            for arg in args {
                bind_expr(db, scope, owner, arg);
            }
        }
        Expr::New { args, .. } | Expr::Tuple(args) | Expr::ArrayLit { elements: args } => {
            for arg in args {
                bind_expr(db, scope, owner, arg);
            }
        }
        Expr::Lambda { params, body } => {
            let lscope = db.alloc_scope(ScopeKind::Function, scope, Span::default(), owner);
            for param in params {
                db.alloc(
                    param.name.clone(),
                    SymbolKind::Parameter,
                    param.value_type.clone(),
                    Span::default(),
                    lscope,
                    None,
                );
            }
            bind_stmts(db, lscope, owner, body);
        }
        Expr::InitList { fields } => {
            for (_, v) in fields {
                bind_expr(db, scope, owner, v);
            }
        }
        Expr::DictLit { pairs } => {
            for (k, v) in pairs {
                bind_expr(db, scope, owner, k);
                bind_expr(db, scope, owner, v);
            }
        }
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => {
            bind_expr(db, scope, owner, cond);
            bind_expr(db, scope, owner, then_expr);
            bind_expr(db, scope, owner, else_expr);
        }
        Expr::Interp { parts } => {
            for part in parts {
                if let crate::ast::InterpPart::Value(v) = part {
                    bind_expr(db, scope, owner, v);
                }
            }
        }
        Expr::Null
        | Expr::Bool(_)
        | Expr::Number(_)
        | Expr::String(_)
        | Expr::Ident(_) => {}
    }
}
