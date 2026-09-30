//! Typed AST lowered from the lossless CST.
//!
//! `lower` is a pure function of the syntax tree. Types are nodes (`Ty`),
//! not strings. The CST remains available for a formatter or language server;
//! this layer is what a later resolver would consume.

use crate::kind::{SyntaxKind, SyntaxNode, SyntaxToken};
use crate::diag::Diagnostic;
use crate::link::{file_stem, is_binding_ident, link_binding_name, LinkTarget};
use crate::span::Span;
use rowan::{NodeOrToken, TextRange};

#[derive(Clone, Debug, PartialEq)]
pub struct Program {
    pub span: Span,
    pub items: Vec<Item>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Item {
    Function(Function),
    Struct(Aggregate),
    Class(Aggregate),
    Interface(Aggregate),
    Enum(Enum),
    Namespace(Namespace),
    TypeAlias(TypeAlias),
    Var(Var),
    Link(Link),
    Directive(Span),
    RejectedModule { form: String, span: Span },
    Error(Span),
}

#[derive(Clone, Debug, PartialEq)]
pub struct Function {
    pub span: Span,
    pub owner: Option<String>,
    pub name: String,
    pub is_constructor: bool,
    pub ret: Option<Ty>,
    pub params: Vec<Param>,
    pub body: bool,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Aggregate {
    pub span: Span,
    pub name: String,
    pub base: Option<Ty>,
    pub fields: Vec<String>,
    pub methods: Vec<String>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Enum {
    pub span: Span,
    pub name: String,
    pub variants: Vec<String>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Namespace {
    pub span: Span,
    pub name: Option<String>,
    pub items: Vec<Item>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct TypeAlias {
    pub span: Span,
    pub name: String,
    pub ty: Ty,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Var {
    pub span: Span,
    pub name: String,
    pub ty: Option<Ty>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Link {
    pub span: Span,
    pub target: LinkTarget,
    pub alias: Option<String>,
    /// True when the source wrote `as`, even if the name is missing.
    pub has_as: bool,
    /// Absent when `as` is missing or invalid, or the stem is not an identifier.
    pub binding: Option<String>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Param {
    pub name: String,
    pub ty: Option<Ty>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Ty {
    pub span: Span,
    pub kind: TyKind,
}

#[derive(Clone, Debug, PartialEq)]
pub enum TyKind {
    Named {
        segments: Vec<String>,
        args: Vec<Ty>,
    },
    Function {
        ret: Box<Ty>,
        params: Vec<Ty>,
    },
    Pointer(Box<Ty>),
    Union(Box<Ty>, Box<Ty>),
    Intersect(Box<Ty>, Box<Ty>),
    Specified {
        specs: Vec<String>,
        inner: Box<Ty>,
    },
    Error,
}

pub fn lower(root: &SyntaxNode) -> Program {
    Program {
        span: span_of_node(root),
        items: root.children().filter_map(|n| lower_item(&n)).collect(),
    }
}

fn lower_item(node: &SyntaxNode) -> Option<Item> {
    Some(match node.kind() {
        SyntaxKind::Function => Item::Function(lower_fn(node)),
        SyntaxKind::Struct => Item::Struct(lower_agg(node)),
        SyntaxKind::Class => Item::Class(lower_agg(node)),
        SyntaxKind::Interface => Item::Interface(lower_agg(node)),
        SyntaxKind::Enum => Item::Enum(lower_enum(node)),
        SyntaxKind::Namespace => Item::Namespace(Namespace {
            span: span_of_node(node),
            name: idents(node).into_iter().next(),
            items: node.children().filter_map(|n| lower_item(&n)).collect(),
        }),
        SyntaxKind::TypeAlias => {
            let names = idents(node);
            Item::TypeAlias(TypeAlias {
                span: span_of_node(node),
                name: names.into_iter().next().unwrap_or_default(),
                ty: node
                    .children()
                    .find(|n| n.kind() == SyntaxKind::Type)
                    .map(|n| lower_ty(&n))
                    .unwrap_or(Ty {
                        span: span_of_node(node),
                        kind: TyKind::Error,
                    }),
            })
        }
        SyntaxKind::Var => Item::Var(Var {
            span: span_of_node(node),
            name: idents(node).into_iter().next().unwrap_or_default(),
            ty: node
                .children()
                .find(|n| n.kind() == SyntaxKind::Type)
                .map(|n| lower_ty(&n)),
        }),
        SyntaxKind::Link => Item::Link(lower_link(node)),
        SyntaxKind::Directive => Item::Directive(span_of_node(node)),
        SyntaxKind::RejectedModule => Item::RejectedModule {
            form: if node.text().to_string().contains("include") {
                "include".into()
            } else {
                "import".into()
            },
            span: span_of_node(node),
        },
        SyntaxKind::Error => Item::Error(span_of_node(node)),
        _ => return None,
    })
}

fn lower_fn(node: &SyntaxNode) -> Function {
    let names = idents(node);
    let has_scope = tokens(node).iter().any(|t| t.kind() == SyntaxKind::ColonColon);
    let ret = node
        .children()
        .find(|n| n.kind() == SyntaxKind::Type)
        .map(|n| lower_ty(&n));
    let (owner, name, is_constructor) = if has_scope && names.len() >= 2 {
        let owner = names[0].clone();
        let name = names[1].clone();
        let ctor = ret.is_none() && owner == name;
        (Some(owner), name, ctor)
    } else {
        (None, names.into_iter().next().unwrap_or_default(), false)
    };
    let params = node
        .children()
        .find(|n| n.kind() == SyntaxKind::ParamList)
        .map(|n| {
            n.children()
                .filter(|c| c.kind() == SyntaxKind::Param)
                .map(|n| lower_param(&n))
                .collect()
        })
        .unwrap_or_default();
    Function {
        span: span_of_node(node),
        owner,
        name,
        is_constructor,
        ret,
        params,
        body: node.children().any(|n| n.kind() == SyntaxKind::Block),
    }
}

fn lower_param(node: &SyntaxNode) -> Param {
    Param {
        name: idents(node).into_iter().next().unwrap_or_default(),
        ty: node
            .children()
            .find(|n| n.kind() == SyntaxKind::Type)
            .map(|n| lower_ty(&n)),
    }
}

fn lower_agg(node: &SyntaxNode) -> Aggregate {
    let mut fields = Vec::new();
    let mut methods = Vec::new();
    for child in node.children() {
        match child.kind() {
            SyntaxKind::Var => {
                if let Some(name) = idents(&child).into_iter().next() {
                    fields.push(name);
                }
            }
            SyntaxKind::Function => {
                if let Some(name) = idents(&child).into_iter().last() {
                    methods.push(name);
                }
            }
            _ => {}
        }
    }
    Aggregate {
        span: span_of_node(node),
        name: idents(node).into_iter().next().unwrap_or_default(),
        base: None,
        fields,
        methods,
    }
}

fn lower_enum(node: &SyntaxNode) -> Enum {
    Enum {
        span: span_of_node(node),
        name: idents(node).into_iter().next().unwrap_or_default(),
        variants: node
            .children()
            .filter(|n| n.kind() == SyntaxKind::EnumVariant)
            .filter_map(|n| idents(&n).into_iter().next())
            .collect(),
    }
}

fn lower_link(node: &SyntaxNode) -> Link {
    let mut alias = None;
    let mut target = LinkTarget::Package(Vec::new());
    let has_as = tokens(node).iter().any(|t| t.kind() == SyntaxKind::KwAs);
    for child in node.children() {
        match child.kind() {
            SyntaxKind::Literal => {
                if let Some(text) = first_token(&child, SyntaxKind::StringLit) {
                    target = LinkTarget::Path(unquote(&text));
                }
            }
            SyntaxKind::NameRef => {
                let segs = child
                    .children()
                    .filter(|n| n.kind() == SyntaxKind::Name)
                    .filter_map(|n| first_ident(&n))
                    .collect();
                target = LinkTarget::Package(segs);
            }
            SyntaxKind::Name if has_as => alias = first_ident(&child),
            _ => {}
        }
    }
    let binding = link_binding_name(&target, alias.as_deref(), has_as);
    Link {
        span: span_of_node(node),
        target,
        alias,
        has_as,
        binding,
    }
}

/// Diagnostics for bindings `lower` recorded but did not judge.
///
/// A second link that reuses a name, and a stem that is not an identifier,
/// are reported here. An `as` clause with no name is already a parse error;
/// its binding stays absent.
pub fn link_diagnostics(program: &Program) -> Vec<Diagnostic> {
    let mut out = Vec::new();
    let mut seen: Vec<(String, Span)> = Vec::new();
    collect_link_diagnostics(&program.items, &mut seen, &mut out);
    out
}

fn collect_link_diagnostics(items: &[Item], seen: &mut Vec<(String, Span)>, out: &mut Vec<Diagnostic>) {
    for item in items {
        match item {
            Item::Link(link) => {
                if link.binding.is_none() && !link.has_as {
                    let raw = match &link.target {
                        LinkTarget::Path(path) => file_stem(path),
                        LinkTarget::Package(segs) => segs.last().cloned().unwrap_or_default(),
                    };
                    let shown = if raw.is_empty() { "module".to_string() } else { raw };
                    out.push(
                        Diagnostic::error(
                            link.span,
                            format!("`{shown}` is not a binding name; write `as Name`"),
                        )
                        .with_code("CLPP0802"),
                    );
                }
                if let Some(name) = &link.binding {
                    if !is_binding_ident(name) {
                        out.push(
                            Diagnostic::error(link.span, format!("`{name}` is not a binding name; write `as Name`"))
                                .with_code("CLPP0802"),
                        );
                    } else if seen.iter().any(|(prev, _)| prev == name) {
                        out.push(
                            Diagnostic::error(link.span, format!("link binding `{name}` is already used in this file"))
                                .with_code("CLPP0802")
                                .with_help("rename one link with `as`"),
                        );
                    } else {
                        seen.push((name.clone(), link.span));
                    }
                }
            }
            Item::Namespace(ns) => collect_link_diagnostics(&ns.items, seen, out),
            _ => {}
        }
    }
}

fn first_ident(node: &SyntaxNode) -> Option<String> {
    first_token(node, SyntaxKind::Ident)
}

fn first_token(node: &SyntaxNode, kind: SyntaxKind) -> Option<String> {
    node.children_with_tokens()
        .filter_map(|e| e.into_token())
        .find(|t| t.kind() == kind)
        .map(|t| t.text().to_string())
}

fn lower_ty(node: &SyntaxNode) -> Ty {
    let span = span_of_node(node);
    let sig = significant(node);
    if let Some(idx) = sig.iter().position(|e| token_kind(e) == Some(SyntaxKind::Pipe)) {
        if let (Some(left), Some(right)) = (type_child_before(&sig, idx), type_child_after(&sig, idx)) {
            return Ty {
                span,
                kind: TyKind::Union(Box::new(lower_ty(&left)), Box::new(lower_ty(&right))),
            };
        }
    }
    if let Some(idx) = sig.iter().position(|e| token_kind(e) == Some(SyntaxKind::Amp)) {
        if let (Some(left), Some(right)) = (type_child_before(&sig, idx), type_child_after(&sig, idx)) {
            return Ty {
                span,
                kind: TyKind::Intersect(Box::new(lower_ty(&left)), Box::new(lower_ty(&right))),
            };
        }
    }
    let specs: Vec<String> = sig
        .iter()
        .filter_map(|e| match e {
            NodeOrToken::Token(t) if is_spec(t.kind()) => Some(t.text().to_string()),
            _ => None,
        })
        .collect();
    if !specs.is_empty() {
        if let Some(inner) = sig.iter().find_map(|e| match e {
            NodeOrToken::Node(n) if n.kind() == SyntaxKind::Type => Some(n.clone()),
            _ => None,
        }) {
            return Ty {
                span,
                kind: TyKind::Specified {
                    specs,
                    inner: Box::new(lower_ty(&inner)),
                },
            };
        }
    }
    let stars = sig
        .iter()
        .filter(|e| token_kind(e) == Some(SyntaxKind::Star))
        .count();
    if stars > 0 {
        if let Some(inner) = sig.iter().find_map(|e| match e {
            NodeOrToken::Node(n) if n.kind() == SyntaxKind::Type => Some(n.clone()),
            _ => None,
        }) {
            let mut ty = lower_ty(&inner);
            for _ in 0..stars {
                ty = Ty {
                    span,
                    kind: TyKind::Pointer(Box::new(ty)),
                };
            }
            return ty;
        }
    }
    if sig.first().and_then(|e| token_text(e)) == Some("function".into()) {
        let types: Vec<Ty> = sig
            .iter()
            .filter_map(|e| match e {
                NodeOrToken::Node(n) if n.kind() == SyntaxKind::Type => Some(lower_ty(n)),
                _ => None,
            })
            .collect();
        if let Some((ret, params)) = types.split_first() {
            return Ty {
                span,
                kind: TyKind::Function {
                    ret: Box::new(ret.clone()),
                    params: params.to_vec(),
                },
            };
        }
    }
    let mut segments = Vec::new();
    let mut args = Vec::new();
    for e in &sig {
        match e {
            NodeOrToken::Token(t) if is_type_word(t.kind()) || t.kind() == SyntaxKind::Ident => {
                segments.push(t.text().to_string());
            }
            NodeOrToken::Node(n) if n.kind() == SyntaxKind::Name => {
                if let Some(text) = n
                    .children_with_tokens()
                    .filter_map(|e| e.into_token())
                    .find(|t| t.kind() == SyntaxKind::Ident)
                {
                    segments.push(text.text().to_string());
                }
            }
            NodeOrToken::Node(n) if n.kind() == SyntaxKind::GenericArgs => {
                args = n
                    .children()
                    .filter(|c| c.kind() == SyntaxKind::Type)
                    .map(|c| lower_ty(&c))
                    .collect();
            }
            _ => {}
        }
    }
    if segments.is_empty() && args.is_empty() {
        return Ty {
            span,
            kind: TyKind::Error,
        };
    }
    Ty {
        span,
        kind: TyKind::Named { segments, args },
    }
}

fn type_child_before(sig: &[NodeOrToken<SyntaxNode, SyntaxToken>], idx: usize) -> Option<SyntaxNode> {
    sig[..idx].iter().rev().find_map(|e| match e {
        NodeOrToken::Node(n) if n.kind() == SyntaxKind::Type => Some(n.clone()),
        _ => None,
    })
}

fn type_child_after(sig: &[NodeOrToken<SyntaxNode, SyntaxToken>], idx: usize) -> Option<SyntaxNode> {
    sig[idx + 1..].iter().find_map(|e| match e {
        NodeOrToken::Node(n) if n.kind() == SyntaxKind::Type => Some(n.clone()),
        _ => None,
    })
}

fn significant(node: &SyntaxNode) -> Vec<NodeOrToken<SyntaxNode, SyntaxToken>> {
    node.children_with_tokens()
        .filter(|e| match e {
            NodeOrToken::Token(t) => !t.kind().is_trivia(),
            NodeOrToken::Node(_) => true,
        })
        .collect()
}

fn token_kind(e: &NodeOrToken<SyntaxNode, SyntaxToken>) -> Option<SyntaxKind> {
    e.as_token().map(|t| t.kind())
}

fn token_text(e: &NodeOrToken<SyntaxNode, SyntaxToken>) -> Option<String> {
    e.as_token().map(|t| t.text().to_string())
}

fn is_spec(kind: SyntaxKind) -> bool {
    matches!(
        kind,
        SyntaxKind::KwConst
            | SyntaxKind::KwStatic
            | SyntaxKind::KwConstexpr
            | SyntaxKind::KwInline
            | SyntaxKind::KwAsync
            | SyntaxKind::KwOverride
    )
}

fn is_type_word(kind: SyntaxKind) -> bool {
    matches!(
        kind,
        SyntaxKind::KwVoid
            | SyntaxKind::KwInt
            | SyntaxKind::KwFloat
            | SyntaxKind::KwDouble
            | SyntaxKind::KwBool
            | SyntaxKind::KwString
            | SyntaxKind::KwAuto
            | SyntaxKind::KwFunc
            | SyntaxKind::KwTuple
            | SyntaxKind::KwVariant
    )
}

fn idents(node: &SyntaxNode) -> Vec<String> {
    let mut names = Vec::new();
    for child in node.children() {
        if child.kind() == SyntaxKind::Name || child.kind() == SyntaxKind::NameRef {
            if let Some(text) = child
                .children_with_tokens()
                .filter_map(|e| e.into_token())
                .find(|t| t.kind() == SyntaxKind::Ident)
            {
                names.push(text.text().to_string());
            }
        }
    }
    names
}

fn tokens(node: &SyntaxNode) -> Vec<SyntaxToken> {
    node.children_with_tokens()
        .filter_map(|e| e.into_token())
        .filter(|t| !t.kind().is_trivia())
        .collect()
}

fn unquote(text: &str) -> String {
    let t = text.trim();
    t.strip_prefix('"')
        .and_then(|s| s.strip_suffix('"'))
        .unwrap_or(t)
        .replace("\\\"", "\"")
        .replace("\\\\", "\\")
}

fn span_of_node(node: &SyntaxNode) -> Span {
    span_of_range(node.text_range())
}

fn span_of_range(range: TextRange) -> Span {
    Span::new(range.start().into(), range.end().into())
}

pub fn dump_cst(node: &SyntaxNode) -> String {
    let mut out = String::new();
    dump_cst_at(node, 0, &mut out);
    out
}

fn dump_cst_at(node: &SyntaxNode, indent: usize, out: &mut String) {
    let pad = "  ".repeat(indent);
    out.push_str(&pad);
    out.push_str(&format!("{:?}", node.kind()));
    out.push('\n');
    for el in node.children_with_tokens() {
        match el {
            NodeOrToken::Token(t) if t.kind().is_trivia() => {}
            NodeOrToken::Token(t) if t.kind() == SyntaxKind::Error && t.text().is_empty() => {}
            NodeOrToken::Token(t) => {
                let text = t.text().to_string();
                if text.chars().all(|c| c.is_whitespace()) {
                    continue;
                }
                out.push_str(&pad);
                out.push_str("  ");
                out.push_str(&format!("{:?} {:?}\n", t.kind(), text));
            }
            NodeOrToken::Node(n) => dump_cst_at(&n, indent + 1, out),
        }
    }
}

pub fn dump_ast(program: &Program) -> String {
    let mut out = String::new();
    for item in &program.items {
        dump_item(item, 0, &mut out);
    }
    out
}

fn dump_item(item: &Item, indent: usize, out: &mut String) {
    let pad = "  ".repeat(indent);
    match item {
        Item::Function(f) => {
            let owner = f
                .owner
                .as_ref()
                .map(|o| format!("{o}::"))
                .unwrap_or_default();
            let ctor = if f.is_constructor { " ctor" } else { "" };
            out.push_str(&format!(
                "{pad}(fn {owner}{}{ctor}{})\n",
                f.name,
                f.ret
                    .as_ref()
                    .map(|t| format!(" : {}", ty_str(t)))
                    .unwrap_or_default()
            ));
        }
        Item::Struct(a) | Item::Class(a) | Item::Interface(a) => {
            let kind = match item {
                Item::Class(_) => "class",
                Item::Interface(_) => "interface",
                _ => "struct",
            };
            out.push_str(&format!("{pad}({kind} {})\n", a.name));
            for field in &a.fields {
                out.push_str(&format!("{pad}  (field {field})\n"));
            }
            for method in &a.methods {
                out.push_str(&format!("{pad}  (method {method})\n"));
            }
        }
        Item::Enum(e) => out.push_str(&format!(
            "{pad}(enum {} {})\n",
            e.name,
            e.variants.join(",")
        )),
        Item::Namespace(n) => {
            out.push_str(&format!(
                "{pad}(namespace {})\n",
                n.name.as_deref().unwrap_or("_")
            ));
            for child in &n.items {
                dump_item(child, indent + 1, out);
            }
        }
        Item::TypeAlias(a) => {
            out.push_str(&format!("{pad}(alias {} = {})\n", a.name, ty_str(&a.ty)))
        }
        Item::Var(v) => out.push_str(&format!(
            "{pad}(var {}{})\n",
            v.name,
            v.ty
                .as_ref()
                .map(|t| format!(" : {}", ty_str(t)))
                .unwrap_or_default()
        )),
        Item::Link(l) => out.push_str(&format!(
            "{pad}(link {} as {})\n",
            l.target.display(),
            l.binding.as_deref().unwrap_or("<error>")
        )),
        Item::Directive(_) => out.push_str(&format!("{pad}(directive)\n")),
        Item::RejectedModule { form, .. } => out.push_str(&format!("{pad}(rejected {form})\n")),
        Item::Error(_) => out.push_str(&format!("{pad}(error)\n")),
    }
}

fn ty_str(ty: &Ty) -> String {
    match &ty.kind {
        TyKind::Named { segments, args } => {
            let base = segments.join("::");
            if args.is_empty() {
                base
            } else {
                format!(
                    "{base}<{}>",
                    args.iter().map(ty_str).collect::<Vec<_>>().join(", ")
                )
            }
        }
        TyKind::Function { ret, params } => format!(
            "function<{}({})>",
            ty_str(ret),
            params.iter().map(ty_str).collect::<Vec<_>>().join(", ")
        ),
        TyKind::Pointer(inner) => format!("{}*", ty_str(inner)),
        TyKind::Union(a, b) => format!("{} | {}", ty_str(a), ty_str(b)),
        TyKind::Intersect(a, b) => format!("{} & {}", ty_str(a), ty_str(b)),
        TyKind::Specified { specs, inner } => format!("{} {}", specs.join(" "), ty_str(inner)),
        TyKind::Error => "?".into(),
    }
}
