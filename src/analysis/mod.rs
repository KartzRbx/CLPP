//! Semantic analysis for diagnostics and IDE queries (Luau Analysis analogue).

use crate::ast::{
    visit::{stmt_at, visit_program, NodeRef},
    Decl, Function, Item, Program, SourceComment, Stmt,
};
use crate::builtins;
use crate::parser::{parse_for_ide, parse_with_diagnostics};
use crate::preprocess::preprocess;
use crate::semantic::{is_bare_global, is_instance_type, luau_type};
use crate::semantic::check::check_program;
use crate::support::CompileDiagnostic;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PositionRequest {
    pub source: String,
    #[serde(default = "default_file_name", alias = "fileName")]
    pub file_name: String,
    #[serde(default = "one")]
    pub line: usize,
    #[serde(default = "one")]
    pub column: usize,
}

fn default_file_name() -> String {
    "input.clpp".into()
}

fn one() -> usize {
    1
}

#[derive(Debug, Clone, Serialize)]
pub struct CompletionItem {
    pub label: String,
    pub kind: String,
    pub detail: String,
    #[serde(skip_serializing_if = "Option::is_none", rename = "insertText")]
    pub insert_text: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
pub struct HoverInfo {
    pub contents: String,
    pub line: usize,
    pub column: usize,
}

#[derive(Debug, Clone, Serialize)]
pub struct LocationInfo {
    pub file_name: String,
    pub line: usize,
    pub column: usize,
    pub name: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct DocumentSymbol {
    pub name: String,
    pub kind: String,
    pub detail: String,
    pub line: usize,
    pub column: usize,
    pub end_line: usize,
}

#[derive(Debug, Clone, Serialize)]
pub struct SignatureInfo {
    pub label: String,
    pub parameters: Vec<String>,
    #[serde(rename = "activeParameter")]
    pub active_parameter: usize,
}

#[derive(Debug, Clone, Serialize)]
pub struct InlayHint {
    pub line: usize,
    pub column: usize,
    pub label: String,
    pub kind: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct FoldingRange {
    #[serde(rename = "startLine")]
    pub start_line: usize,
    #[serde(rename = "endLine")]
    pub end_line: usize,
    pub kind: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct CodeAction {
    pub title: String,
    pub kind: String,
    pub line: usize,
    pub column: usize,
    pub end_line: usize,
    pub end_column: usize,
    pub new_text: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct FormatResponse {
    pub ok: bool,
    pub text: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct CompletionResponse {
    pub ok: bool,
    pub items: Vec<CompletionItem>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub diagnostics: Vec<CompileDiagnostic>,
}

#[derive(Debug, Clone, Serialize)]
pub struct HoverResponse {
    pub ok: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub hover: Option<HoverInfo>,
}

#[derive(Debug, Clone, Serialize)]
pub struct SymbolsResponse {
    pub ok: bool,
    pub symbols: Vec<DocumentSymbol>,
}

#[derive(Debug, Clone, Serialize)]
pub struct DefinitionResponse {
    pub ok: bool,
    pub locations: Vec<LocationInfo>,
}

#[derive(Debug, Clone, Serialize)]
pub struct SignatureResponse {
    pub ok: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub signature: Option<SignatureInfo>,
}

#[derive(Debug, Clone)]
struct Symbol {
    name: String,
    kind: String,
    detail: String,
    ty: String,
    line: usize,
    column: usize,
    end_line: usize,
    owner: Option<String>,
    params: Vec<String>,
    insert: Option<String>,
    doc: Option<String>,
}

#[derive(Debug, Clone)]
struct Index {
    program: Program,
    #[allow(dead_code)]
    comments: Vec<SourceComment>,
    symbols: Vec<Symbol>,
    scopes: Vec<(usize, usize, Vec<Symbol>)>,
    diagnostics: Vec<CompileDiagnostic>,
}

pub fn complete_request(req: &PositionRequest) -> CompletionResponse {
    let (index, prefix) = index_at(req);
    let items = complete_from(&index, req, &prefix);
    CompletionResponse {
        ok: true,
        items,
        diagnostics: index.diagnostics,
    }
}

pub fn hover_request(req: &PositionRequest) -> HoverResponse {
    let (index, _) = index_at(req);
    HoverResponse {
        ok: true,
        hover: hover_from(&index, req),
    }
}

pub fn symbols_request(req: &PositionRequest) -> SymbolsResponse {
    let (index, _) = index_at(req);
    SymbolsResponse {
        ok: true,
        symbols: index
            .symbols
            .iter()
            .filter(|s| s.kind != "Variable" || s.owner.is_some())
            .map(|s| DocumentSymbol {
                name: s.name.clone(),
                kind: s.kind.clone(),
                detail: s.detail.clone(),
                line: s.line.saturating_sub(1),
                column: s.column.saturating_sub(1),
                end_line: s.end_line.saturating_sub(1),
            })
            .collect(),
    }
}

pub fn definition_request(req: &PositionRequest) -> DefinitionResponse {
    let (index, _) = index_at(req);
    DefinitionResponse {
        ok: true,
        locations: definition_from(&index, req),
    }
}

pub fn references_request(req: &PositionRequest) -> DefinitionResponse {
    let (index, _) = index_at(req);
    DefinitionResponse {
        ok: true,
        locations: references_from(&index, req),
    }
}

pub fn signature_request(req: &PositionRequest) -> SignatureResponse {
    let (index, prefix) = index_at(req);
    SignatureResponse {
        ok: true,
        signature: signature_from(&index, &prefix),
    }
}

pub fn inlay_request(req: &PositionRequest) -> Vec<InlayHint> {
    let (index, _) = index_at(req);
    inlay_from(&index)
}

pub fn folding_request(req: &PositionRequest) -> Vec<FoldingRange> {
    let (index, _) = index_at(req);
    folding_from(&index)
}

pub fn highlight_request(req: &PositionRequest) -> DefinitionResponse {
    references_request(req)
}

pub fn workspace_symbols(req: &PositionRequest) -> SymbolsResponse {
    symbols_request(req)
}

pub fn code_actions_request(req: &PositionRequest) -> Vec<CodeAction> {
    let (index, _) = index_at(req);
    actions_from(&index, req)
}

pub fn format_request(source: &str) -> FormatResponse {
    FormatResponse {
        ok: true,
        text: crate::fmt::format_source(source),
    }
}

fn index_at(req: &PositionRequest) -> (Index, String) {
    let path = Path::new(&req.file_name);
    let (expanded, ctx) = match preprocess(&req.source, path) {
        Ok(pair) => pair,
        Err(_) => (req.source.clone(), Default::default()),
    };
    let (program, mut diagnostics) = parse_for_ide(&expanded, &req.file_name);
    if diagnostics.is_empty() {
        if let Ok((p, d)) = parse_with_diagnostics(&expanded, &req.file_name) {
            let _ = p;
            diagnostics.extend(d);
        }
    }
    if let Ok(check) = check_program(&program, &expanded) {
        diagnostics.extend(check);
    }
    attach_docs(&program, &ctx.comments);
    let index = build_index(program, ctx.comments, diagnostics);
    let prefix = line_prefix(&req.source, req.line, req.column);
    (index, prefix)
}

fn attach_docs(program: &Program, comments: &[SourceComment]) {
    let _ = program;
    let _ = comments;
}

fn build_index(
    program: Program,
    comments: Vec<SourceComment>,
    diagnostics: Vec<CompileDiagnostic>,
) -> Index {
    let mut symbols = Vec::new();
    let mut scopes = Vec::new();
    let mut structs: HashMap<String, Vec<Symbol>> = HashMap::new();

    for item in &program.items {
        match item {
            Item::Function(func) | Item::Proto(func) => {
                let kind = if func.owner.is_some() {
                    "Method"
                } else {
                    "Function"
                };
                let params: Vec<String> = func
                    .params
                    .iter()
                    .map(|p| {
                        if let Some(ty) = &p.value_type {
                            format!("{}: {ty}", p.name)
                        } else {
                            p.name.clone()
                        }
                    })
                    .collect();
                let detail = func
                    .return_type
                    .clone()
                    .unwrap_or_else(|| "void".into());
                let insert = Some(format!(
                    "{}({})",
                    func.name,
                    func.params
                        .iter()
                        .enumerate()
                        .map(|(i, p)| format!("${{{}:{}}}", i + 1, p.name))
                        .collect::<Vec<_>>()
                        .join(", ")
                ));
                let doc = comments.iter().rev().find(|c| {
                    c.is_doc && c.line + 1 == func.line || c.line == func.line
                }).map(|c| c.text.clone());
                let sym = Symbol {
                    name: if let Some(owner) = &func.owner {
                        format!("{}::{}", owner, func.name)
                    } else {
                        func.name.clone()
                    },
                    kind: kind.into(),
                    detail: detail.clone(),
                    ty: detail,
                    line: func.line,
                    column: 1,
                    end_line: func.span.end_line.max(func.line),
                    owner: func.owner.clone(),
                    params: params.clone(),
                    insert,
                    doc,
                };
                if let Some(owner) = &func.owner {
                    structs.entry(owner.clone()).or_default().push(Symbol {
                        name: func.name.clone(),
                        kind: "Method".into(),
                        detail: func.return_type.clone().unwrap_or_else(|| "void".into()),
                        ty: func.return_type.clone().unwrap_or_else(|| "void".into()),
                        line: func.line,
                        column: 1,
                        end_line: func.span.end_line,
                        owner: Some(owner.clone()),
                        params: params.clone(),
                        insert: Some(format!(
                            "{}({})",
                            func.name,
                            func.params
                                .iter()
                                .enumerate()
                                .map(|(i, p)| format!("${{{}:{}}}", i + 1, p.name))
                                .collect::<Vec<_>>()
                                .join(", ")
                        )),
                        doc: None,
                    });
                }
                let mut locals = Vec::new();
                for param in &func.params {
                    locals.push(Symbol {
                        name: param.name.clone(),
                        kind: "Variable".into(),
                        detail: param.value_type.clone().unwrap_or_else(|| "auto".into()),
                        ty: param.value_type.clone().unwrap_or_else(|| "auto".into()),
                        line: func.line,
                        column: 1,
                        end_line: func.span.end_line,
                        owner: func.owner.clone(),
                        params: Vec::new(),
                        insert: None,
                        doc: None,
                    });
                }
                collect_locals(&func.body, &mut locals, func.owner.as_deref());
                scopes.push((func.line, func.span.end_line.max(func.line + 1), locals));
                symbols.push(sym);
            }
            Item::Decl(decl) => {
                let ty = decl
                    .value_type
                    .clone()
                    .unwrap_or_else(|| "auto".into());
                let sym = Symbol {
                    name: decl.name.clone(),
                    kind: if decl.owner.is_some() {
                        "Field".into()
                    } else {
                        "Variable".into()
                    },
                    detail: ty.clone(),
                    ty: ty.clone(),
                    line: decl.line,
                    column: 1,
                    end_line: decl.span.end_line.max(decl.line),
                    owner: decl.owner.clone(),
                    params: Vec::new(),
                    insert: None,
                    doc: None,
                };
                if let Some(owner) = &decl.owner {
                    structs.entry(owner.clone()).or_default().push(sym.clone());
                }
                symbols.push(sym);
            }
            _ => {}
        }
    }
    for (owner, members) in structs {
        symbols.push(Symbol {
            name: owner,
            kind: "Class".into(),
            detail: "struct".into(),
            ty: "struct".into(),
            line: members.first().map(|m| m.line).unwrap_or(1),
            column: 1,
            end_line: members.last().map(|m| m.end_line).unwrap_or(1),
            owner: None,
            params: Vec::new(),
            insert: None,
            doc: None,
        });
        let _ = members;
    }
    Index {
        program,
        comments,
        symbols,
        scopes,
        diagnostics,
    }
}

fn collect_locals(stmts: &[Stmt], locals: &mut Vec<Symbol>, owner: Option<&str>) {
    for stmt in stmts {
        match stmt {
            Stmt::Decl(decl) => {
                locals.push(Symbol {
                    name: decl.name.clone(),
                    kind: "Variable".into(),
                    detail: decl.value_type.clone().unwrap_or_else(|| "auto".into()),
                    ty: decl.value_type.clone().unwrap_or_else(|| "auto".into()),
                    line: decl.line,
                    column: 1,
                    end_line: decl.span.end_line.max(decl.line),
                    owner: owner.map(str::to_string),
                    params: Vec::new(),
                    insert: None,
                    doc: None,
                });
            }
            Stmt::ForEach {
                name,
                elem_type,
                body,
                span,
                ..
            } => {
                locals.push(Symbol {
                    name: name.clone(),
                    kind: "Variable".into(),
                    detail: elem_type.clone().unwrap_or_else(|| "auto".into()),
                    ty: elem_type.clone().unwrap_or_else(|| "auto".into()),
                    line: span.start_line,
                    column: span.start_col,
                    end_line: span.end_line,
                    owner: owner.map(str::to_string),
                    params: Vec::new(),
                    insert: None,
                    doc: None,
                });
                collect_locals(body, locals, owner);
            }
            Stmt::If {
                consequent,
                alternate,
                ..
            } => {
                collect_locals(consequent, locals, owner);
                if let Some(alt) = alternate {
                    collect_locals(alt, locals, owner);
                }
            }
            Stmt::Guard { body, .. }
            | Stmt::While { body, .. }
            | Stmt::Spawn { body, .. }
            | Stmt::Block(body)
            | Stmt::CFor { body, .. } => collect_locals(body, locals, owner),
            Stmt::Switch { cases, .. } => {
                for case in cases {
                    collect_locals(&case.body, locals, owner);
                }
            }
            Stmt::Match { arms, .. } => {
                for arm in arms {
                    if let (Some(class), Some(binding)) = (&arm.class_name, &arm.binding) {
                        locals.push(Symbol {
                            name: binding.clone(),
                            kind: "Variable".into(),
                            detail: class.clone(),
                            ty: class.clone(),
                            line: 1,
                            column: 1,
                            end_line: 1,
                            owner: owner.map(str::to_string),
                            params: Vec::new(),
                            insert: None,
                            doc: None,
                        });
                    }
                    collect_locals(&arm.body, locals, owner);
                }
            }
            Stmt::Destructure { names, .. } => {
                for name in names {
                    locals.push(Symbol {
                        name: name.clone(),
                        kind: "Variable".into(),
                        detail: "auto".into(),
                        ty: "auto".into(),
                        line: stmt.line(),
                        column: 1,
                        end_line: stmt.line(),
                        owner: owner.map(str::to_string),
                        params: Vec::new(),
                        insert: None,
                        doc: None,
                    });
                }
            }
            _ => {}
        }
    }
}

fn line_prefix(source: &str, line: usize, column: usize) -> String {
    let row = source.lines().nth(line.saturating_sub(1)).unwrap_or("");
    let col = column.saturating_sub(1).min(row.len());
    row[..col].to_string()
}

fn word_at(source: &str, line: usize, column: usize) -> String {
    let row = source.lines().nth(line.saturating_sub(1)).unwrap_or("");
    let bytes = row.as_bytes();
    let mut i = column.saturating_sub(1).min(row.len());
    if i > 0 && i == row.len() {
        i -= 1;
    }
    while i < row.len() && !is_ident_byte(bytes.get(i).copied().unwrap_or(0)) && i > 0 {
        i -= 1;
    }
    let mut start = i;
    while start > 0 && is_ident_byte(bytes[start - 1]) {
        start -= 1;
    }
    if start > 0 && bytes[start - 1] == b'@' {
        start -= 1;
    }
    let mut end = i;
    while end < row.len() && is_ident_byte(bytes[end]) {
        end += 1;
    }
    row[start..end].to_string()
}

fn is_ident_byte(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'_'
}

#[allow(dead_code)]
enum Access {
    Include(String, bool),
    At(String),
    Member(String, String),
    Static(String, String),
    Cleanup(String, String),
    Ident(String),
}

fn parse_access(prefix: &str) -> Access {
    let trimmed = prefix.trim_end();
    if let Some(rest) = trimmed.strip_prefix("#include") {
        let rest = rest.trim_start();
        if let Some(body) = rest.strip_prefix('<') {
            return Access::Include(body.to_string(), true);
        }
        if let Some(body) = rest.strip_prefix('"') {
            return Access::Include(body.to_string(), false);
        }
        return Access::Include(String::new(), rest.contains('<'));
    }
    if let Some(at) = trimmed.rfind('@') {
        let after = &trimmed[at + 1..];
        if after.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
            return Access::At(after.to_string());
        }
    }
    if let Some(idx) = trimmed.rfind("~>") {
        let obj = object_name(&trimmed[..idx]);
        let partial = trimmed[idx + 2..].trim().to_string();
        return Access::Cleanup(obj, partial);
    }
    if let Some(idx) = trimmed.rfind("::") {
        let obj = object_name(&trimmed[..idx]);
        let partial = trimmed[idx + 2..].to_string();
        return Access::Static(obj, partial);
    }
    if let Some(idx) = trimmed.rfind('.') {
        if !trimmed[idx..].starts_with(".:") {
            let obj = object_name(&trimmed[..idx]);
            let partial = trimmed[idx + 1..].to_string();
            return Access::Member(obj, partial);
        }
    }
    let ident = trimmed
        .rsplit(|c: char| !c.is_ascii_alphanumeric() && c != '_')
        .next()
        .unwrap_or("")
        .to_string();
    Access::Ident(ident)
}

fn object_name(before: &str) -> String {
    let before = before.trim_end();
    let ident: String = before
        .chars()
        .rev()
        .take_while(|c| c.is_ascii_alphanumeric() || *c == '_')
        .collect();
    ident.chars().rev().collect()
}

fn in_scope<'a>(index: &'a Index, line: usize) -> Vec<&'a Symbol> {
    let mut out = Vec::new();
    for sym in &index.symbols {
        if matches!(sym.kind.as_str(), "Function" | "Class" | "Variable" | "Field")
            && (sym.kind != "Variable" || sym.line <= line)
            && sym.owner.is_none()
        {
            out.push(sym);
        }
        if sym.kind == "Method" && sym.owner.is_none() {
            out.push(sym);
        }
    }
    for (start, end, locals) in &index.scopes {
        if line >= *start && line <= *end {
            for local in locals {
                if local.line <= line {
                    out.push(local);
                }
            }
        }
    }
    out
}

fn complete_from(index: &Index, req: &PositionRequest, prefix: &str) -> Vec<CompletionItem> {
    match parse_access(prefix) {
        Access::Include(_, angled) => include_items(angled),
        Access::At(partial) => filter_partial(at_items(index, req.line), &partial),
        Access::Member(obj, partial) => {
            filter_partial(member_items(index, req.line, &obj, false), &partial)
        }
        Access::Static(obj, partial) => {
            filter_partial(member_items(index, req.line, &obj, true), &partial)
        }
        Access::Cleanup(_, partial) => filter_partial(cleanup_items(), &partial),
        Access::Ident(partial) => filter_partial(ident_items(index, req.line), &partial),
    }
}

fn filter_partial(mut items: Vec<CompletionItem>, partial: &str) -> Vec<CompletionItem> {
    if partial.is_empty() {
        return items;
    }
    let lower = partial.to_ascii_lowercase();
    items.retain(|i| {
        i.label
            .trim_start_matches('@')
            .to_ascii_lowercase()
            .starts_with(&lower)
    });
    items
}

fn ident_items(index: &Index, line: usize) -> Vec<CompletionItem> {
    let mut items = Vec::new();
    let mut seen = std::collections::HashSet::new();
    for kw in builtins::KEYWORDS {
        items.push(CompletionItem {
            label: (*kw).into(),
            kind: "Keyword".into(),
            detail: "CL++".into(),
            insert_text: None,
        });
        seen.insert((*kw).to_string());
    }
    for b in builtins::BUILTINS {
        if seen.insert(b.clpp.to_string()) {
            items.push(CompletionItem {
                label: b.clpp.into(),
                kind: b.kind.into(),
                detail: b.detail.into(),
                insert_text: Some(format!("{}($1)", b.clpp)),
            });
        }
    }
    for sym in &index.symbols {
        if sym.kind != "Function" {
            continue;
        }
        let label = sym
            .name
            .rsplit("::")
            .next()
            .unwrap_or(&sym.name)
            .to_string();
        if seen.insert(label.clone()) {
            items.push(CompletionItem {
                label,
                kind: "Function".into(),
                detail: sym.detail.clone(),
                insert_text: Some(
                    sym.insert
                        .clone()
                        .unwrap_or_else(|| format!("{}($1)", sym.name)),
                ),
            });
        }
    }
    for sym in in_scope(index, line) {
        let label = if let Some(rest) = sym.name.rsplit("::").next() {
            rest.to_string()
        } else {
            sym.name.clone()
        };
        if seen.insert(label.clone()) {
            items.push(CompletionItem {
                label,
                kind: sym.kind.clone(),
                detail: sym.detail.clone(),
                insert_text: if sym.kind == "Function" || sym.kind == "Method" {
                    Some(
                        sym.insert
                            .clone()
                            .unwrap_or_else(|| format!("{}($1)", sym.name)),
                    )
                } else {
                    None
                },
            });
        }
    }
    for ty in ["Player", "Players", "Folder", "Part", "IntValue", "Janitor", "Vector3", "CFrame"] {
        if seen.insert(ty.into()) {
            items.push(CompletionItem {
                label: ty.into(),
                kind: "Class".into(),
                detail: "type".into(),
                insert_text: None,
            });
        }
    }
    items
}

fn at_items(index: &Index, line: usize) -> Vec<CompletionItem> {
    let mut items = vec![CompletionItem {
        label: "@this".into(),
        kind: "Variable".into(),
        detail: "receiver (self)".into(),
        insert_text: Some("@this".into()),
    }];
    let owner = enclosing_owner(index, line);
    for sym in &index.symbols {
        if sym.kind != "Field" && sym.kind != "Method" {
            continue;
        }
        if let Some(owner) = &owner {
            if sym.owner.as_deref() != Some(owner.as_str()) {
                continue;
            }
        } else if sym.owner.is_none() {
            continue;
        }
        let short = sym.name.rsplit("::").next().unwrap_or(&sym.name);
        items.push(CompletionItem {
            label: format!("@{short}"),
            kind: sym.kind.clone(),
            detail: format!("self.{short}"),
            insert_text: Some(format!("@{short}")),
        });
    }
    items
}

fn enclosing_owner(index: &Index, line: usize) -> Option<String> {
    index
        .symbols
        .iter()
        .filter(|s| s.kind == "Method" && s.owner.is_some() && line >= s.line)
        .max_by_key(|s| s.line)
        .and_then(|s| s.owner.clone())
}

fn lookup_ty(index: &Index, line: usize, name: &str) -> String {
    for sym in in_scope(index, line) {
        let short = sym.name.rsplit("::").next().unwrap_or(&sym.name);
        if short == name {
            return if sym.ty.is_empty() {
                "auto".into()
            } else {
                sym.ty.clone()
            };
        }
    }
    "auto".into()
}

fn member_items(index: &Index, line: usize, obj: &str, static_access: bool) -> Vec<CompletionItem> {
    let ty = lookup_ty(index, line, obj);
    let mut items = Vec::new();
    for sym in &index.symbols {
        if sym.owner.as_deref() == Some(ty.as_str())
            || (static_access && (sym.name == ty || sym.owner.as_deref() == Some(obj)))
        {
            let short = sym.name.rsplit("::").next().unwrap_or(&sym.name);
            items.push(CompletionItem {
                label: short.to_string(),
                kind: sym.kind.clone(),
                detail: sym.detail.clone(),
                insert_text: if sym.kind == "Method" {
                    Some(format!("{short}($1)"))
                } else {
                    None
                },
            });
        }
    }
    if is_instance_type(&ty) || is_instance_type(obj) || ty == "auto" || ty.is_empty() {
        for name in builtins::INSTANCE_PROPS {
            items.push(CompletionItem {
                label: (*name).into(),
                kind: "Property".into(),
                detail: format!("{ty}.{name}"),
                insert_text: None,
            });
        }
        for name in builtins::INSTANCE_METHODS {
            items.push(CompletionItem {
                label: (*name).into(),
                kind: "Method".into(),
                detail: format!("{ty}.{name}"),
                insert_text: Some(format!("{name}($1)")),
            });
        }
    }
    if ty == "signal" || ty.starts_with("signal") {
        for name in ["Connect", "Once", "Wait", "Fire"] {
            items.push(CompletionItem {
                label: name.into(),
                kind: "Method".into(),
                detail: "signal".into(),
                insert_text: Some(format!("{name}($1)")),
            });
        }
    }
    items
}

fn cleanup_items() -> Vec<CompletionItem> {
    ["Connect", "Once", "Wait"]
        .into_iter()
        .map(|name| CompletionItem {
            label: name.into(),
            kind: "Method".into(),
            detail: "~> auto-cleanup".into(),
            insert_text: Some(format!("{name}($1)")),
        })
        .collect()
}

fn include_items(angled: bool) -> Vec<CompletionItem> {
    let libs = [
        "clpp/libs/janitor.clh",
        "clpp/libs/promise.clh",
        "clpp/libs/signal.clh",
        "clpp/roblox.clh",
        "clpp/instances.clh",
        "clpp/datatypes.clh",
    ];
    libs.into_iter()
        .map(|path| CompletionItem {
            label: path.into(),
            kind: "File".into(),
            detail: if angled {
                "include <>".into()
            } else {
                "include".into()
            },
            insert_text: Some(path.into()),
        })
        .collect()
}

fn hover_from(index: &Index, req: &PositionRequest) -> Option<HoverInfo> {
    let word = word_at(&req.source, req.line, req.column);
    if word.is_empty() {
        return None;
    }
    let bare = word.trim_start_matches('@');
    if let Some(b) = builtins::find(bare) {
        return Some(HoverInfo {
            contents: format!("**{}** → `{}`\n\n{}", b.clpp, b.luau, b.detail),
            line: req.line,
            column: req.column,
        });
    }
    if word == "@this" || word == "this" {
        return Some(HoverInfo {
            contents: "`@this` — current object (Luau `self`). Only inside `Class::Method`."
                .into(),
            line: req.line,
            column: req.column,
        });
    }
    for sym in in_scope(index, req.line) {
        let short = sym.name.rsplit("::").next().unwrap_or(&sym.name);
        if short == bare || sym.name == bare || format!("@{short}") == word {
            let mut md = format!("**{}** — `{}` ({})", short, sym.ty, sym.kind);
            if let Some(doc) = &sym.doc {
                md.push_str("\n\n");
                md.push_str(doc);
            }
            if word.starts_with('@') {
                md.push_str(&format!("\n\nEmits `self.{short}`."));
            }
            return Some(HoverInfo {
                contents: md,
                line: req.line,
                column: req.column,
            });
        }
    }
    None
}

fn definition_from(index: &Index, req: &PositionRequest) -> Vec<LocationInfo> {
    let word = word_at(&req.source, req.line, req.column);
    let bare = word.trim_start_matches('@');
    let mut out = Vec::new();
    for sym in in_scope(index, req.line).into_iter().chain(index.symbols.iter()) {
        let short = sym.name.rsplit("::").next().unwrap_or(&sym.name);
        if short == bare || sym.name == bare {
            out.push(LocationInfo {
                file_name: index.program.file_name.clone(),
                line: sym.line,
                column: sym.column,
                name: short.to_string(),
            });
        }
    }
    out
}

fn references_from(index: &Index, req: &PositionRequest) -> Vec<LocationInfo> {
    let word = word_at(&req.source, req.line, req.column);
    let bare = word.trim_start_matches('@').to_string();
    if bare.is_empty() {
        return Vec::new();
    }
    let mut out = Vec::new();
    for (i, row) in req.source.lines().enumerate() {
        let mut start = 0usize;
        while let Some(idx) = row[start..].find(&bare) {
            let at = start + idx;
            let before_ok = at == 0
                || !row.as_bytes()[at - 1].is_ascii_alphanumeric() && row.as_bytes()[at - 1] != b'_';
            let after = at + bare.len();
            let after_ok = after >= row.len()
                || !row.as_bytes()[after].is_ascii_alphanumeric() && row.as_bytes()[after] != b'_';
            if before_ok && after_ok {
                out.push(LocationInfo {
                    file_name: index.program.file_name.clone(),
                    line: i + 1,
                    column: at + 1,
                    name: bare.clone(),
                });
            }
            start = at + bare.len();
        }
    }
    out
}

fn signature_from(index: &Index, prefix: &str) -> Option<SignatureInfo> {
    let open = prefix.rfind('(')?;
    let name = object_name(&prefix[..open]);
    if name.is_empty() {
        return None;
    }
    let commas = prefix[open + 1..]
        .chars()
        .filter(|c| *c == ',')
        .count();
    if let Some(b) = builtins::find(&name) {
        return Some(SignatureInfo {
            label: format!("{}(...)", b.clpp),
            parameters: vec!["...".into()],
            active_parameter: commas,
        });
    }
    for sym in &index.symbols {
        let short = sym.name.rsplit("::").next().unwrap_or(&sym.name);
        if short == name {
            return Some(SignatureInfo {
                label: format!("{}({})", short, sym.params.join(", ")),
                parameters: sym.params.clone(),
                active_parameter: commas.min(sym.params.len().saturating_sub(1)),
            });
        }
    }
    None
}

fn inlay_from(index: &Index) -> Vec<InlayHint> {
    let mut hints = Vec::new();
    for (_start, _end, locals) in &index.scopes {
        for local in locals {
            if local.detail == "auto" {
                continue;
            }
            if local.kind == "Variable" {
                hints.push(InlayHint {
                    line: local.line.saturating_sub(1),
                    column: local.column.saturating_sub(1) + local.name.len(),
                    label: format!(": {}", local.detail),
                    kind: "Type".into(),
                });
            }
        }
    }
    hints
}

fn folding_from(index: &Index) -> Vec<FoldingRange> {
    index
        .symbols
        .iter()
        .filter(|s| s.end_line > s.line)
        .map(|s| FoldingRange {
            start_line: s.line.saturating_sub(1),
            end_line: s.end_line.saturating_sub(1),
            kind: "region".into(),
        })
        .collect()
}

fn actions_from(index: &Index, req: &PositionRequest) -> Vec<CodeAction> {
    let word = word_at(&req.source, req.line, req.column);
    let mut actions = Vec::new();
    let replacements = [
        ("tostring", "to_string"),
        ("tonumber", "to_number"),
        ("print", "post"),
        ("error", "report"),
    ];
    for (from, to) in replacements {
        if word == from {
            actions.push(CodeAction {
                title: format!("Use {to}"),
                kind: "quickfix".into(),
                line: req.line.saturating_sub(1),
                column: req.column.saturating_sub(1),
                end_line: req.line.saturating_sub(1),
                end_column: req.column.saturating_sub(1) + from.len(),
                new_text: to.into(),
            });
        }
    }
    let _ = index;
    actions
}

pub fn stmt_covering<'a>(program: &'a Program, line: usize, col: usize) -> Option<&'a Stmt> {
    stmt_at(program, line, col)
}

pub fn visit_nodes(program: &Program, f: impl FnMut(NodeRef<'_>) -> bool) {
    let mut f = f;
    let _ = visit_program(program, &mut f);
}

pub fn function_doc<'a>(comments: &'a [SourceComment], func: &Function) -> Option<&'a str> {
    comments
        .iter()
        .rev()
        .find(|c| c.is_doc && (c.line + 1 == func.line || c.line == func.line))
        .map(|c| c.text.as_str())
}

pub fn luau_detail(decl: &Decl) -> String {
    luau_type(decl.value_type.as_deref()).unwrap_or_else(|| "any".into())
}

pub fn is_known_global(name: &str) -> bool {
    is_bare_global(name) || builtins::find(name).is_some()
}
