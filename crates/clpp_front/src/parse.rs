//! Recursive-descent parser with Pratt expressions.
//!
//! `parse_tokens` is a pure function of the source and the token slice. It
//! always returns a tree. Tokens, including whitespace and comments, are
//! placed in the tree exactly once so `syntax.text()` equals the source.
//!
//! Expression layers follow the existing grammar, low to high:
//! assign, try (`?`), colon-call, ternary, then Pratt for `??` through `**`.

use crate::diag::Diagnostic;
use crate::kind::{Lang, SyntaxKind, SyntaxNode};
use crate::lex::Token;
use crate::span::Span;
use rowan::{GreenNodeBuilder, Language};

struct Parser<'a> {
    source: &'a str,
    tokens: &'a [Token],
    i: usize,
    elems: Vec<Elem>,
    diags: Vec<Diagnostic>,
}

enum Elem {
    Token { kind: SyntaxKind, span: Span },
    Node { kind: SyntaxKind, children: Vec<u32>, span: Span },
}

struct Cp {
    i: usize,
    elems: usize,
    diags: usize,
}

pub fn parse_tokens(source: &str, tokens: &[Token]) -> (SyntaxNode, Vec<Diagnostic>) {
    let mut p = Parser {
        source,
        tokens,
        i: 0,
        elems: Vec::new(),
        diags: Vec::new(),
    };
    let root = p.parse_source();
    if p.i < p.tokens.len() {
        let mut rest = Vec::new();
        while p.i < p.tokens.len() {
            rest.push(p.push_raw());
        }
        p.error_msg("unexpected trailing input");
        let err = p.node(SyntaxKind::Error, rest);
        p.append_children(root, vec![err]);
    }
    let syntax = to_rowan(source, &p.elems, root);
    (syntax, p.diags)
}

fn to_rowan(source: &str, elems: &[Elem], root: u32) -> SyntaxNode {
    let mut builder = GreenNodeBuilder::new();
    emit(&mut builder, source, elems, root);
    SyntaxNode::new_root(builder.finish())
}

fn emit(builder: &mut GreenNodeBuilder<'_>, source: &str, elems: &[Elem], id: u32) {
    match &elems[id as usize] {
        Elem::Token { kind, span } => {
            let text = source
                .get(span.start as usize..span.end as usize)
                .unwrap_or("");
            builder.token(Lang::kind_to_raw(*kind), text);
        }
        Elem::Node { kind, children, .. } => {
            builder.start_node(Lang::kind_to_raw(*kind));
            for &child in children {
                emit(builder, source, elems, child);
            }
            builder.finish_node();
        }
    }
}

impl<'a> Parser<'a> {
    fn parse_source(&mut self) -> u32 {
        let mut kids = Vec::new();
        while !self.eof() {
            let before = self.i;
            if self.can_start_item() {
                kids.push(self.parse_item());
            } else {
                self.error_msg("unexpected token");
                kids.push(self.recover_item());
            }
            if self.i == before && !self.eof() {
                kids.extend(self.bump());
            }
        }
        while self.i < self.tokens.len() && self.tokens[self.i].kind.is_trivia() {
            kids.push(self.push_raw());
        }
        self.node(SyntaxKind::SourceFile, kids)
    }

    fn can_start_item(&self) -> bool {
        if self.at_attr() || self.out_of_class_fn() || self.type_start() {
            return true;
        }
        matches!(
            self.nth(0),
            SyntaxKind::Hash
                | SyntaxKind::KwImport
                | SyntaxKind::KwLink
                | SyntaxKind::KwNamespace
                | SyntaxKind::KwUsing
                | SyntaxKind::KwType
                | SyntaxKind::KwEnum
                | SyntaxKind::KwStruct
                | SyntaxKind::KwClass
                | SyntaxKind::KwInterface
                | SyntaxKind::KwExtern
                | SyntaxKind::KwTypedef
                | SyntaxKind::KwTemplate
        )
    }

    fn parse_item(&mut self) -> u32 {
        match self.nth(0) {
            SyntaxKind::Hash => self.parse_hash(),
            SyntaxKind::KwImport => self.parse_rejected("import"),
            SyntaxKind::KwLink => self.parse_link(),
            SyntaxKind::KwNamespace => self.parse_namespace(),
            SyntaxKind::KwUsing => self.parse_using(),
            SyntaxKind::KwType => self.parse_type_alias(),
            SyntaxKind::KwEnum => self.parse_enum(),
            SyntaxKind::KwExtern | SyntaxKind::KwTypedef => {
                self.error_msg("this declaration is not supported");
                self.recover_item()
            }
            _ => self.parse_templated_item(),
        }
    }

    fn parse_templated_item(&mut self) -> u32 {
        let mut prefix = Vec::new();
        while self.at_attr() {
            prefix.push(self.parse_attr());
        }
        if self.at(SyntaxKind::KwTemplate) {
            prefix.push(self.parse_template());
            while self.at_attr() {
                prefix.push(self.parse_attr());
            }
        }
        if self.at(SyntaxKind::KwStruct) {
            let id = self.parse_aggregate(SyntaxKind::Struct);
            self.prepend(id, prefix);
            return id;
        }
        if self.at(SyntaxKind::KwClass) {
            let id = self.parse_aggregate(SyntaxKind::Class);
            self.prepend(id, prefix);
            return id;
        }
        if self.at(SyntaxKind::KwInterface) {
            let id = self.parse_aggregate(SyntaxKind::Interface);
            self.prepend(id, prefix);
            return id;
        }
        if self.at(SyntaxKind::KwEnum) {
            let id = self.parse_enum();
            self.prepend(id, prefix);
            return id;
        }
        if self.out_of_class_fn() {
            let id = self.parse_out_of_class();
            self.prepend(id, prefix);
            return id;
        }
        if self.type_start() {
            let id = self.parse_fn_or_var(Vec::new());
            self.prepend(id, prefix);
            return id;
        }
        self.error_msg("expected a declaration");
        let mut kids = prefix;
        kids.push(self.recover_item());
        self.node(SyntaxKind::Error, kids)
    }

    fn parse_hash(&mut self) -> u32 {
        let sig = self.sig_index();
        let start = self.tokens.get(sig).map(|t| t.span.start).unwrap_or(0);
        let bytes = self.source.as_bytes();
        let mut end = start as usize;
        while end < bytes.len() && bytes[end] != b'\n' {
            end += 1;
        }
        let line = &self.source[start as usize..end];
        let rest = line.trim_start().trim_start_matches('#').trim_start();
        let include = is_include_directive(rest);
        if include {
            self.diags.push(
                Diagnostic::error(
                    Span::new(start, end as u32),
                    "`#include` was removed; `link` is the only module form",
                )
                .with_code("CLPP0801")
                .with_help("write `link \"./file.clh\" as Name;`"),
            );
        }
        let kids = self.consume_until(end as u32);
        self.node(
            if include {
                SyntaxKind::RejectedModule
            } else {
                SyntaxKind::Directive
            },
            kids,
        )
    }

    fn parse_rejected(&mut self, form: &str) -> u32 {
        let span = self.sig_span().unwrap_or_else(|| Span::empty(self.here()));
        self.diags.push(
            Diagnostic::error(
                span,
                format!("`{form}` was removed; `link` is the only module form"),
            )
            .with_code("CLPP0801")
            .with_help("write `link \"./file.clh\" as Name;`"),
        );
        let kids = self.recover_balanced();
        self.node(SyntaxKind::RejectedModule, kids)
    }

    fn parse_link(&mut self) -> u32 {
        let mut kids = self.bump(); // link
        if self.at(SyntaxKind::StringLit) || self.at(SyntaxKind::At) {
            kids.push(self.parse_link_target());
        } else {
            self.error_msg("expected a string path or @package after `link`");
        }
        if self.at(SyntaxKind::KwAs) {
            kids.extend(self.bump());
            kids.push(self.parse_name());
        }
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(SyntaxKind::Link, kids)
    }

    fn parse_link_target(&mut self) -> u32 {
        if self.at(SyntaxKind::StringLit) {
            let kids = self.bump();
            return self.node(SyntaxKind::Literal, kids);
        }
        let mut kids = self.bump(); // @
        kids.push(self.parse_name());
        while self.at(SyntaxKind::Dot) {
            kids.extend(self.bump());
            kids.push(self.parse_name());
        }
        self.node(SyntaxKind::NameRef, kids)
    }

    fn parse_namespace(&mut self) -> u32 {
        let mut kids = self.bump();
        if self.at(SyntaxKind::Ident) {
            kids.push(self.parse_name());
        }
        kids.extend(self.expect(SyntaxKind::LBrace));
        while !self.at(SyntaxKind::RBrace) && !self.eof() {
            let before = self.i;
            if self.can_start_item() {
                kids.push(self.parse_item());
            } else {
                self.error_msg("unexpected token in namespace");
                kids.push(self.recover_item());
            }
            if self.i == before && !self.eof() {
                kids.extend(self.bump());
            }
        }
        kids.extend(self.expect(SyntaxKind::RBrace));
        self.node(SyntaxKind::Namespace, kids)
    }

    fn parse_using(&mut self) -> u32 {
        let mut kids = self.bump();
        if self.at(SyntaxKind::KwNamespace) {
            self.error_msg("`using namespace` is not a core declaration; use `namespace` or `link`");
            kids.push(self.recover_item());
            return self.node(SyntaxKind::Error, kids);
        }
        kids.push(self.parse_name());
        kids.extend(self.expect(SyntaxKind::Eq));
        kids.push(self.parse_type());
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(SyntaxKind::TypeAlias, kids)
    }

    fn parse_type_alias(&mut self) -> u32 {
        let mut kids = self.bump(); // type
        kids.push(self.parse_name());
        if self.at(SyntaxKind::Lt) {
            kids.push(self.parse_generic_args());
        }
        kids.extend(self.expect(SyntaxKind::Eq));
        kids.push(self.parse_type());
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(SyntaxKind::TypeAlias, kids)
    }

    fn parse_enum(&mut self) -> u32 {
        let mut kids = self.bump();
        if self.at(SyntaxKind::KwClass) {
            kids.extend(self.bump());
        }
        kids.push(self.parse_name());
        if self.at(SyntaxKind::Colon) && self.nth(1) != SyntaxKind::Colon {
            kids.extend(self.bump());
            kids.push(self.parse_type());
        }
        kids.extend(self.expect(SyntaxKind::LBrace));
        while !self.at(SyntaxKind::RBrace) && !self.eof() {
            if self.at(SyntaxKind::Comma) {
                kids.extend(self.bump());
                continue;
            }
            if !self.at(SyntaxKind::Ident) {
                self.error_msg("expected an enum variant");
                kids.push(self.recover_item());
                if self.at(SyntaxKind::RBrace) {
                    break;
                }
                continue;
            }
            let mut variant = vec![self.parse_name()];
            if self.at(SyntaxKind::Eq) {
                variant.extend(self.bump());
                variant.push(self.parse_expr());
            }
            kids.push(self.node(SyntaxKind::EnumVariant, variant));
            if self.at(SyntaxKind::Comma) {
                kids.extend(self.bump());
            }
        }
        kids.extend(self.expect(SyntaxKind::RBrace));
        if self.at(SyntaxKind::Semi) {
            kids.extend(self.bump());
        } else {
            self.error_msg("expected `;` after enum");
        }
        self.node(SyntaxKind::Enum, kids)
    }

    fn parse_aggregate(&mut self, kind: SyntaxKind) -> u32 {
        let mut kids = self.bump();
        kids.push(self.parse_name());
        if self.at(SyntaxKind::Lt) {
            kids.push(self.parse_generic_args());
        }
        if self.at(SyntaxKind::Colon) && self.nth(1) != SyntaxKind::Colon {
            kids.extend(self.bump());
            kids.push(self.parse_type());
        }
        kids.extend(self.expect(SyntaxKind::LBrace));
        while !self.at(SyntaxKind::RBrace) && !self.eof() {
            let before = self.i;
            if self.is_access() {
                kids.extend(self.bump());
                kids.extend(self.expect(SyntaxKind::Colon));
            } else if self.at(SyntaxKind::Semi) {
                let empty = self.bump();
                kids.push(self.node(SyntaxKind::Empty, empty));
            } else if self.at(SyntaxKind::KwStruct) || self.at(SyntaxKind::KwClass) {
                let k = if self.at(SyntaxKind::KwStruct) {
                    SyntaxKind::Struct
                } else {
                    SyntaxKind::Class
                };
                kids.push(self.parse_aggregate(k));
            } else if self.at(SyntaxKind::Ident) && self.nth(1) == SyntaxKind::LParen {
                let mut ctor = vec![self.parse_name()];
                ctor.push(self.parse_params());
                if self.at(SyntaxKind::LBrace) {
                    ctor.push(self.parse_block());
                } else {
                    ctor.extend(self.expect(SyntaxKind::Semi));
                }
                kids.push(self.node(SyntaxKind::Function, ctor));
            } else if self.type_start() {
                kids.push(self.parse_fn_or_var(Vec::new()));
            } else {
                self.error_msg("expected a field or method");
                kids.push(self.recover_item());
            }
            if self.i == before && !self.eof() && !self.at(SyntaxKind::RBrace) {
                kids.extend(self.bump());
            }
        }
        kids.extend(self.expect(SyntaxKind::RBrace));
        if self.at(SyntaxKind::Semi) {
            kids.extend(self.bump());
        } else {
            self.error_msg("expected `;` after declaration");
        }
        self.node(kind, kids)
    }

    fn parse_template(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::Lt));
        if !self.at(SyntaxKind::Gt) {
            kids.push(self.parse_template_param());
            while self.at(SyntaxKind::Comma) {
                kids.extend(self.bump());
                if self.at(SyntaxKind::Gt) {
                    break;
                }
                kids.push(self.parse_template_param());
            }
        }
        kids.extend(self.expect(SyntaxKind::Gt));
        self.node(SyntaxKind::TemplateHead, kids)
    }

    fn parse_template_param(&mut self) -> u32 {
        let mut kids = Vec::new();
        if self.at(SyntaxKind::KwTypename) || self.at(SyntaxKind::KwClass) {
            kids.extend(self.bump());
        } else {
            self.error_msg("expected `typename` or `class`");
        }
        kids.push(self.parse_name());
        if self.at(SyntaxKind::Colon) && self.nth(1) != SyntaxKind::Colon {
            kids.extend(self.bump());
            kids.push(self.parse_name());
        }
        if self.at(SyntaxKind::Eq) {
            kids.extend(self.bump());
            kids.push(self.parse_type());
        }
        self.node(SyntaxKind::TemplateParam, kids)
    }

    fn parse_attr(&mut self) -> u32 {
        let mut kids = self.expect(SyntaxKind::LBracket);
        kids.extend(self.expect(SyntaxKind::LBracket));
        kids.push(self.parse_name());
        if self.at(SyntaxKind::LParen) {
            kids.extend(self.bump());
            if self.at(SyntaxKind::StringLit) {
                kids.extend(self.bump());
            } else {
                self.error_msg("expected a string in attribute");
            }
            kids.extend(self.expect(SyntaxKind::RParen));
        }
        kids.extend(self.expect(SyntaxKind::RBracket));
        kids.extend(self.expect(SyntaxKind::RBracket));
        self.node(SyntaxKind::Attribute, kids)
    }

    fn parse_out_of_class(&mut self) -> u32 {
        let mut kids = vec![self.parse_name()];
        kids.extend(self.bump()); // ::
        kids.push(self.parse_name());
        kids.push(self.parse_params());
        if self.at(SyntaxKind::LBrace) {
            kids.push(self.parse_block());
        } else {
            kids.extend(self.expect(SyntaxKind::Semi));
        }
        self.node(SyntaxKind::Function, kids)
    }

    fn parse_fn_or_var(&mut self, mut kids: Vec<u32>) -> u32 {
        if self.out_of_class_fn() {
            let id = self.parse_out_of_class();
            self.prepend(id, kids);
            return id;
        }
        if !self.type_start() {
            self.error_msg("expected a type");
            kids.push(self.recover_item());
            return self.node(SyntaxKind::Error, kids);
        }
        kids.push(self.parse_type());
        if !self.at(SyntaxKind::Ident) {
            self.error_msg("expected a name");
            kids.push(self.recover_item());
            return self.node(SyntaxKind::Error, kids);
        }
        kids.push(self.parse_name());
        if self.at(SyntaxKind::ColonColon) {
            kids.extend(self.bump());
            kids.push(self.parse_name());
            kids.push(self.parse_params());
            if self.at(SyntaxKind::LBrace) {
                kids.push(self.parse_block());
            } else {
                kids.extend(self.expect(SyntaxKind::Semi));
            }
            return self.node(SyntaxKind::Function, kids);
        }
        if self.at(SyntaxKind::LParen) {
            kids.push(self.parse_params());
            if self.at(SyntaxKind::LBrace) {
                kids.push(self.parse_block());
            } else {
                kids.extend(self.expect(SyntaxKind::Semi));
            }
            return self.node(SyntaxKind::Function, kids);
        }
        if self.at(SyntaxKind::Eq) {
            kids.extend(self.bump());
            kids.push(self.parse_expr());
        }
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(SyntaxKind::Var, kids)
    }

    fn parse_block(&mut self) -> u32 {
        let mut kids = self.expect(SyntaxKind::LBrace);
        while !self.at(SyntaxKind::RBrace) && !self.eof() {
            let before = self.i;
            kids.push(self.parse_stmt());
            if self.i == before && !self.eof() {
                kids.extend(self.bump());
            }
        }
        kids.extend(self.expect(SyntaxKind::RBrace));
        self.node(SyntaxKind::Block, kids)
    }

    fn parse_stmt(&mut self) -> u32 {
        match self.nth(0) {
            SyntaxKind::KwIf => self.parse_if(),
            SyntaxKind::KwGuard => self.parse_guard(),
            SyntaxKind::KwMatch => self.parse_match(),
            SyntaxKind::KwSwitch => self.parse_switch(),
            SyntaxKind::KwTry => self.parse_try(),
            SyntaxKind::KwFor => self.parse_for(),
            SyntaxKind::KwWhile => self.parse_while(),
            SyntaxKind::KwDo => self.parse_do_while(),
            SyntaxKind::KwReturn => self.parse_return(),
            SyntaxKind::KwBreak => self.parse_semi_stmt(SyntaxKind::Break),
            SyntaxKind::KwContinue => self.parse_semi_stmt(SyntaxKind::Continue),
            SyntaxKind::KwComptime => self.parse_comptime(),
            SyntaxKind::LBrace => self.parse_block(),
            SyntaxKind::Semi => {
                let kids = self.bump();
                self.node(SyntaxKind::Empty, kids)
            }
            SyntaxKind::KwAuto
                if self.nth(1) == SyntaxKind::LBracket || self.nth(1) == SyntaxKind::LBrace =>
            {
                self.parse_destructure()
            }
            _ if self.looks_like_decl() => self.parse_fn_or_var(Vec::new()),
            _ => self.parse_expr_stmt(),
        }
    }

    fn parse_if(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::LParen));
        kids.push(self.parse_expr());
        kids.extend(self.expect(SyntaxKind::RParen));
        kids.push(self.parse_stmt());
        if self.at(SyntaxKind::KwElse) {
            kids.extend(self.bump());
            kids.push(self.parse_stmt());
        }
        self.node(SyntaxKind::If, kids)
    }

    fn parse_guard(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::LParen));
        kids.push(self.parse_expr());
        kids.extend(self.expect(SyntaxKind::RParen));
        kids.extend(self.expect(SyntaxKind::KwElse));
        kids.push(self.parse_stmt());
        self.node(SyntaxKind::Guard, kids)
    }

    fn parse_match(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::LParen));
        kids.push(self.parse_expr());
        kids.extend(self.expect(SyntaxKind::RParen));
        kids.extend(self.expect(SyntaxKind::LBrace));
        while !self.at(SyntaxKind::RBrace) && !self.eof() {
            if self.at(SyntaxKind::Comma) {
                kids.extend(self.bump());
                continue;
            }
            kids.push(self.parse_match_arm());
            if self.at(SyntaxKind::Comma) {
                kids.extend(self.bump());
            }
        }
        kids.extend(self.expect(SyntaxKind::RBrace));
        if self.at(SyntaxKind::Semi) {
            kids.extend(self.bump());
        }
        self.node(SyntaxKind::Match, kids)
    }

    fn parse_match_arm(&mut self) -> u32 {
        let mut kids = Vec::new();
        if self.at(SyntaxKind::Ident) && self.nth(1) == SyntaxKind::FatArrow {
            let name = self.bump();
            kids.push(self.node(SyntaxKind::NameRef, name));
        } else if self.type_start() {
            let cp = self.cp();
            let ty = self.parse_type();
            if self.at(SyntaxKind::Ident) {
                kids.push(ty);
                kids.push(self.parse_name());
            } else {
                self.rewind(cp);
                let name = self.bump();
                kids.push(self.node(SyntaxKind::NameRef, name));
            }
        } else {
            self.error_msg("expected a match pattern");
            kids.extend(self.bump());
        }
        kids.extend(self.expect(SyntaxKind::FatArrow));
        if self.at(SyntaxKind::LBrace) {
            kids.push(self.parse_block());
        } else {
            kids.push(self.parse_expr());
        }
        self.node(SyntaxKind::MatchArm, kids)
    }

    fn parse_switch(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::LParen));
        kids.push(self.parse_expr());
        kids.extend(self.expect(SyntaxKind::RParen));
        kids.extend(self.expect(SyntaxKind::LBrace));
        while !self.at(SyntaxKind::RBrace) && !self.eof() {
            if self.at(SyntaxKind::KwCase) {
                kids.extend(self.bump());
                kids.push(self.parse_expr());
                kids.extend(self.expect(SyntaxKind::Colon));
            } else if self.at(SyntaxKind::KwDefault) {
                kids.extend(self.bump());
                kids.extend(self.expect(SyntaxKind::Colon));
            } else {
                let before = self.i;
                kids.push(self.parse_stmt());
                if self.i == before && !self.eof() {
                    kids.extend(self.bump());
                }
            }
        }
        kids.extend(self.expect(SyntaxKind::RBrace));
        self.node(SyntaxKind::Switch, kids)
    }

    fn parse_try(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.push(self.parse_block());
        kids.extend(self.expect(SyntaxKind::KwCatch));
        kids.extend(self.expect(SyntaxKind::LParen));
        kids.push(self.parse_type());
        kids.push(self.parse_name());
        kids.extend(self.expect(SyntaxKind::RParen));
        kids.push(self.parse_block());
        self.node(SyntaxKind::Try, kids)
    }

    fn parse_for(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::LParen));
        if self.looks_like_range() {
            kids.push(self.parse_type());
            kids.push(self.parse_name());
            if self.at(SyntaxKind::KwIn) || self.at(SyntaxKind::Colon) {
                kids.extend(self.bump());
            } else {
                self.error_msg("expected `in` or `:`");
            }
            kids.push(self.parse_range());
            kids.extend(self.expect(SyntaxKind::RParen));
            kids.push(self.parse_stmt());
            return self.node(SyntaxKind::For, kids);
        }
        if !self.at(SyntaxKind::Semi) {
            if self.looks_like_for_init() {
                kids.push(self.parse_for_init());
            } else {
                kids.push(self.parse_expr());
            }
        }
        kids.extend(self.expect(SyntaxKind::Semi));
        if !self.at(SyntaxKind::Semi) {
            kids.push(self.parse_expr());
        }
        kids.extend(self.expect(SyntaxKind::Semi));
        if !self.at(SyntaxKind::RParen) {
            kids.push(self.parse_expr());
        }
        kids.extend(self.expect(SyntaxKind::RParen));
        kids.push(self.parse_stmt());
        self.node(SyntaxKind::For, kids)
    }

    fn parse_for_init(&mut self) -> u32 {
        let mut kids = vec![self.parse_type(), self.parse_name()];
        if self.at(SyntaxKind::Eq) {
            kids.extend(self.bump());
            kids.push(self.parse_expr());
        }
        self.node(SyntaxKind::Var, kids)
    }

    fn parse_range(&mut self) -> u32 {
        let mut kids = vec![self.parse_expr()];
        if self.at(SyntaxKind::Range) || self.at(SyntaxKind::RangeExclusive) {
            kids.extend(self.bump());
            kids.push(self.parse_expr());
            if self.at(SyntaxKind::KwBy) {
                kids.extend(self.bump());
                kids.push(self.parse_expr());
            }
        }
        self.node(SyntaxKind::RangeExpr, kids)
    }

    fn parse_while(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::LParen));
        kids.push(self.parse_expr());
        kids.extend(self.expect(SyntaxKind::RParen));
        kids.push(self.parse_stmt());
        self.node(SyntaxKind::While, kids)
    }

    fn parse_do_while(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.push(self.parse_stmt());
        kids.extend(self.expect(SyntaxKind::KwWhile));
        kids.extend(self.expect(SyntaxKind::LParen));
        kids.push(self.parse_expr());
        kids.extend(self.expect(SyntaxKind::RParen));
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(SyntaxKind::DoWhile, kids)
    }

    fn parse_return(&mut self) -> u32 {
        let mut kids = self.bump();
        if !self.at(SyntaxKind::Semi) && self.can_start_expr() {
            kids.push(self.parse_expr());
            while self.at(SyntaxKind::Comma) && self.can_start_expr_after(1) {
                kids.extend(self.bump());
                kids.push(self.parse_expr());
            }
        }
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(SyntaxKind::Return, kids)
    }

    fn parse_semi_stmt(&mut self, kind: SyntaxKind) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(kind, kids)
    }

    fn parse_comptime(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.push(self.parse_block());
        if self.at(SyntaxKind::Semi) {
            kids.extend(self.bump());
        }
        self.node(SyntaxKind::Comptime, kids)
    }

    fn parse_destructure(&mut self) -> u32 {
        let mut kids = self.bump(); // auto
        let open = self.nth(0);
        kids.extend(self.bump());
        while !self.at(SyntaxKind::RBracket) && !self.at(SyntaxKind::RBrace) && !self.eof() {
            if self.at(SyntaxKind::Comma) {
                kids.extend(self.bump());
                continue;
            }
            if self.at(SyntaxKind::Ident) {
                kids.push(self.parse_name());
            } else {
                self.error_msg("expected a name");
                break;
            }
        }
        if open == SyntaxKind::LBracket {
            kids.extend(self.expect(SyntaxKind::RBracket));
        } else {
            kids.extend(self.expect(SyntaxKind::RBrace));
        }
        kids.extend(self.expect(SyntaxKind::Eq));
        kids.push(self.parse_expr());
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(SyntaxKind::Destructure, kids)
    }

    fn parse_expr_stmt(&mut self) -> u32 {
        let mut kids = vec![self.parse_expr()];
        kids.extend(self.expect(SyntaxKind::Semi));
        self.node(SyntaxKind::ExprStmt, kids)
    }

    fn parse_expr(&mut self) -> u32 {
        self.parse_assign()
    }

    fn parse_assign(&mut self) -> u32 {
        let lhs = self.parse_try_expr();
        if self.is_assign_op() {
            let mut kids = vec![lhs];
            kids.extend(self.bump());
            kids.push(self.parse_assign());
            self.node(SyntaxKind::Assign, kids)
        } else {
            lhs
        }
    }

    fn parse_try_expr(&mut self) -> u32 {
        let lhs = self.parse_colon_expr();
        if self.at(SyntaxKind::Question) {
            let mut kids = vec![lhs];
            kids.extend(self.bump());
            self.node(SyntaxKind::Try, kids)
        } else {
            lhs
        }
    }

    fn parse_colon_expr(&mut self) -> u32 {
        let mut lhs = self.parse_ternary();
        while self.at(SyntaxKind::Colon) && self.nth(1) == SyntaxKind::Ident {
            let mut kids = vec![lhs];
            kids.extend(self.bump());
            kids.push(self.parse_name());
            if self.at(SyntaxKind::Lt) && self.generic_call_from_here() {
                kids.push(self.parse_generic_args());
            }
            if self.at(SyntaxKind::LParen) {
                kids.push(self.parse_args());
            }
            lhs = self.node(SyntaxKind::Member, kids);
        }
        lhs
    }

    fn parse_ternary(&mut self) -> u32 {
        let cond = self.parse_bp(0);
        if !self.at(SyntaxKind::Question) {
            return cond;
        }
        let cp = self.cp();
        let q = self.bump();
        let mid = self.parse_ternary();
        if self.at(SyntaxKind::Colon) {
            let mut kids = vec![cond];
            kids.extend(q);
            kids.push(mid);
            kids.extend(self.bump());
            kids.push(self.parse_ternary());
            self.node(SyntaxKind::Ternary, kids)
        } else {
            self.rewind(cp);
            cond
        }
    }

    fn parse_bp(&mut self, min: u8) -> u32 {
        let mut lhs = self.parse_prefix();
        loop {
            let Some((lbp, rbp)) = self.infix_bp() else {
                break;
            };
            if lbp < min {
                break;
            }
            let mut kids = vec![lhs];
            kids.extend(self.bump());
            kids.push(self.parse_bp(rbp));
            lhs = self.node(SyntaxKind::Binary, kids);
        }
        lhs
    }

    fn parse_prefix(&mut self) -> u32 {
        if self.at(SyntaxKind::KwAwait) {
            let mut kids = self.bump();
            kids.push(self.parse_bp(29));
            return self.node(SyntaxKind::Await, kids);
        }
        if self.at(SyntaxKind::Bang) || self.at(SyntaxKind::Minus) {
            let mut kids = self.bump();
            kids.push(self.parse_bp(29));
            return self.node(SyntaxKind::Unary, kids);
        }
        self.parse_primary()
    }

    fn parse_primary(&mut self) -> u32 {
        let mut lhs = self.parse_atom();
        loop {
            if self.at(SyntaxKind::LParen) {
                let mut kids = vec![lhs];
                kids.push(self.parse_args());
                lhs = self.node(SyntaxKind::Call, kids);
            } else if self.at(SyntaxKind::LBracket) && !self.at_attr() {
                let mut kids = vec![lhs];
                kids.extend(self.bump());
                kids.push(self.parse_expr());
                kids.extend(self.expect(SyntaxKind::RBracket));
                lhs = self.node(SyntaxKind::Index, kids);
            } else if self.at(SyntaxKind::Dot) {
                lhs = self.parse_member(lhs, false);
            } else if self.at(SyntaxKind::QuestionDot) {
                lhs = self.parse_member(lhs, true);
            } else if self.at(SyntaxKind::ColonColon) {
                let mut kids = vec![lhs];
                kids.extend(self.bump());
                kids.push(self.parse_name());
                if self.at(SyntaxKind::Lt) && self.generic_call_from_here() {
                    kids.push(self.parse_generic_args());
                }
                if self.at(SyntaxKind::LParen) {
                    kids.push(self.parse_args());
                }
                lhs = self.node(SyntaxKind::Member, kids);
            } else if self.at(SyntaxKind::KwAs) {
                let mut kids = vec![lhs];
                kids.extend(self.bump());
                kids.push(self.parse_type());
                lhs = self.node(SyntaxKind::Cast, kids);
            } else if self.at(SyntaxKind::PlusPlus) || self.at(SyntaxKind::MinusMinus) {
                let mut kids = vec![lhs];
                kids.extend(self.bump());
                lhs = self.node(SyntaxKind::Unary, kids);
            } else if self.at(SyntaxKind::LBrace) {
                let mut kids = vec![lhs];
                kids.push(self.parse_init());
                lhs = self.node(SyntaxKind::Call, kids);
            } else {
                break;
            }
        }
        lhs
    }

    fn parse_member(&mut self, lhs: u32, question: bool) -> u32 {
        let mut kids = vec![lhs];
        kids.extend(self.bump());
        if self.at(SyntaxKind::Ident) {
            kids.push(self.parse_name());
        } else {
            self.error_msg("expected a member name");
        }
        if self.at(SyntaxKind::Lt) && self.generic_call_from_here() {
            kids.push(self.parse_generic_args());
        }
        if self.at(SyntaxKind::LParen) {
            kids.push(self.parse_args());
        }
        let _ = question;
        self.node(SyntaxKind::Member, kids)
    }

    fn parse_atom(&mut self) -> u32 {
        if self.at(SyntaxKind::KwFunc) && self.nth(1) == SyntaxKind::LParen {
            return self.parse_lambda();
        }
        if self.at(SyntaxKind::KwNew) {
            return self.parse_new();
        }
        if self.at(SyntaxKind::Backtick) {
            return self.parse_template_string();
        }
        if self.at(SyntaxKind::LParen) {
            let mut kids = self.bump();
            kids.push(self.parse_expr());
            kids.extend(self.expect(SyntaxKind::RParen));
            return self.node(SyntaxKind::Paren, kids);
        }
        if self.at(SyntaxKind::LBrace) {
            return self.parse_init();
        }
        if self.at(SyntaxKind::At) {
            let mut kids = self.bump();
            if self.at(SyntaxKind::Ident) {
                kids.push(self.parse_name());
            } else {
                self.error_msg("expected a name after `@`");
            }
            return self.node(SyntaxKind::AtExpr, kids);
        }
        if self.is_named_cast() {
            return self.parse_named_cast();
        }
        if self.at(SyntaxKind::Ident) && self.nth(1) == SyntaxKind::Lt && self.generic_on_ident() {
            let name = self.bump();
            let mut kids = vec![self.node(SyntaxKind::Name, name)];
            kids.push(self.parse_generic_args());
            if self.at(SyntaxKind::LParen) {
                kids.push(self.parse_args());
                return self.node(SyntaxKind::Call, kids);
            }
            return self.node(SyntaxKind::NameRef, kids);
        }
        if self.is_literal() || self.at(SyntaxKind::Ident) {
            let kind = if self.at(SyntaxKind::Ident) {
                SyntaxKind::NameRef
            } else {
                SyntaxKind::Literal
            };
            let kids = self.bump();
            return self.node(kind, kids);
        }
        self.error_msg("expected an expression");
        if self.eof() || self.at_expr_boundary() {
            self.node(SyntaxKind::Error, Vec::new())
        } else {
            let kids = self.bump();
            self.node(SyntaxKind::Error, kids)
        }
    }

    fn parse_lambda(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.push(self.parse_params());
        kids.push(self.parse_block());
        self.node(SyntaxKind::Lambda, kids)
    }

    fn parse_new(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.push(self.parse_name());
        if self.at(SyntaxKind::LParen) {
            kids.push(self.parse_args());
        }
        self.node(SyntaxKind::New, kids)
    }

    fn parse_named_cast(&mut self) -> u32 {
        let mut kids = self.bump();
        kids.extend(self.expect(SyntaxKind::Lt));
        kids.push(self.parse_type());
        kids.extend(self.expect(SyntaxKind::Gt));
        kids.extend(self.expect(SyntaxKind::LParen));
        kids.push(self.parse_expr());
        kids.extend(self.expect(SyntaxKind::RParen));
        self.node(SyntaxKind::NamedCast, kids)
    }

    fn parse_template_string(&mut self) -> u32 {
        let mut kids = self.bump(); // opening backtick
        loop {
            if self.at(SyntaxKind::TemplateText) {
                kids.extend(self.bump());
            } else if self.at(SyntaxKind::LBrace) {
                kids.extend(self.bump());
                kids.push(self.parse_expr());
                kids.extend(self.expect(SyntaxKind::RBrace));
            } else if self.at(SyntaxKind::Backtick) {
                kids.extend(self.bump());
                break;
            } else {
                self.error_msg("unterminated template string");
                break;
            }
        }
        if self.at(SyntaxKind::Comma) && self.can_start_expr_after(1) {
            while self.at(SyntaxKind::Comma) && self.can_start_expr_after(1) {
                kids.extend(self.bump());
                kids.push(self.parse_expr());
            }
            return self.node(SyntaxKind::StringJoin, kids);
        }
        self.node(SyntaxKind::Template, kids)
    }

    fn parse_init(&mut self) -> u32 {
        let mut kids = self.expect(SyntaxKind::LBrace);
        if !self.at(SyntaxKind::RBrace) {
            if self.at(SyntaxKind::Dot) {
                while !self.at(SyntaxKind::RBrace) && !self.eof() {
                    if self.at(SyntaxKind::Comma) {
                        kids.extend(self.bump());
                        continue;
                    }
                    kids.extend(self.expect(SyntaxKind::Dot));
                    kids.push(self.parse_name());
                    kids.extend(self.expect(SyntaxKind::Eq));
                    kids.push(self.parse_expr());
                    if self.at(SyntaxKind::Comma) {
                        kids.extend(self.bump());
                    } else {
                        break;
                    }
                }
            } else if self.at(SyntaxKind::LBrace) {
                while !self.at(SyntaxKind::RBrace) && !self.eof() {
                    if self.at(SyntaxKind::Comma) {
                        kids.extend(self.bump());
                        continue;
                    }
                    kids.extend(self.expect(SyntaxKind::LBrace));
                    kids.push(self.parse_expr());
                    kids.extend(self.expect(SyntaxKind::Comma));
                    kids.push(self.parse_expr());
                    kids.extend(self.expect(SyntaxKind::RBrace));
                    if self.at(SyntaxKind::Comma) {
                        kids.extend(self.bump());
                    } else {
                        break;
                    }
                }
            } else {
                kids.push(self.parse_expr());
                while self.at(SyntaxKind::Comma) {
                    kids.extend(self.bump());
                    if self.at(SyntaxKind::RBrace) {
                        break;
                    }
                    kids.push(self.parse_expr());
                }
            }
        }
        kids.extend(self.expect(SyntaxKind::RBrace));
        self.node(SyntaxKind::Init, kids)
    }

    fn parse_args(&mut self) -> u32 {
        let mut kids = self.expect(SyntaxKind::LParen);
        if !self.at(SyntaxKind::RParen) && !self.eof() {
            kids.push(self.parse_expr());
            while self.at(SyntaxKind::Comma) {
                kids.extend(self.bump());
                if self.at(SyntaxKind::RParen) {
                    break;
                }
                kids.push(self.parse_expr());
            }
        }
        kids.extend(self.expect(SyntaxKind::RParen));
        self.node(SyntaxKind::ArgList, kids)
    }

    fn parse_params(&mut self) -> u32 {
        let mut kids = self.expect(SyntaxKind::LParen);
        if !self.at(SyntaxKind::RParen) && !self.eof() {
            loop {
                if self.at(SyntaxKind::RParen) {
                    break;
                }
                let before = self.i;
                kids.push(self.parse_param());
                if self.at(SyntaxKind::Comma) {
                    kids.extend(self.bump());
                    continue;
                }
                if self.i == before && !self.eof() {
                    kids.extend(self.bump());
                }
                break;
            }
        }
        kids.extend(self.expect(SyntaxKind::RParen));
        self.node(SyntaxKind::ParamList, kids)
    }

    fn parse_param(&mut self) -> u32 {
        if self.at(SyntaxKind::Ident) && self.nth(1) == SyntaxKind::Colon {
            let mut kids = vec![self.parse_name()];
            kids.extend(self.bump());
            kids.push(self.parse_type());
            if self.at(SyntaxKind::Eq) {
                kids.extend(self.bump());
                kids.push(self.parse_expr());
            }
            return self.node(SyntaxKind::Param, kids);
        }
        let mut kids = vec![self.parse_type()];
        if self.at(SyntaxKind::Ident) {
            kids.push(self.parse_name());
        } else {
            self.error_msg("expected a parameter name");
        }
        if self.at(SyntaxKind::Eq) {
            kids.extend(self.bump());
            kids.push(self.parse_expr());
        }
        self.node(SyntaxKind::Param, kids)
    }

    fn parse_type(&mut self) -> u32 {
        let mut left = self.parse_type_prefixed();
        while self.at(SyntaxKind::Pipe) || self.at(SyntaxKind::Amp) {
            let mut kids = vec![left];
            kids.extend(self.bump());
            kids.push(self.parse_type_prefixed());
            left = self.node(SyntaxKind::Type, kids);
        }
        left
    }

    fn parse_type_prefixed(&mut self) -> u32 {
        if !self.is_specifier() {
            return self.parse_type_atom();
        }
        let mut kids = Vec::new();
        while self.is_specifier() {
            kids.extend(self.bump());
        }
        kids.push(self.parse_type_atom());
        self.node(SyntaxKind::Type, kids)
    }

    fn parse_type_atom(&mut self) -> u32 {
        let mut kids = Vec::new();
        if self.at(SyntaxKind::Ident)
            && self.nth_text(0) == "function"
            && self.nth(1) == SyntaxKind::Lt
        {
            kids.extend(self.bump());
            kids.extend(self.expect(SyntaxKind::Lt));
            kids.push(self.parse_type());
            kids.extend(self.expect(SyntaxKind::LParen));
            if !self.at(SyntaxKind::RParen) && !self.eof() {
                kids.push(self.parse_type());
                while self.at(SyntaxKind::Comma) {
                    kids.extend(self.bump());
                    if self.at(SyntaxKind::RParen) {
                        break;
                    }
                    kids.push(self.parse_type());
                }
            }
            kids.extend(self.expect(SyntaxKind::RParen));
            kids.extend(self.expect(SyntaxKind::Gt));
        } else if self.at_type_name() {
            kids.extend(self.bump());
            while self.at(SyntaxKind::ColonColon) {
                kids.extend(self.bump());
                if self.at_type_name() {
                    kids.extend(self.bump());
                } else {
                    self.error_msg("expected a name after `::`");
                    break;
                }
            }
            if self.at(SyntaxKind::Lt) && self.scan_generic().is_some() {
                kids.push(self.parse_generic_args());
            }
        } else {
            self.error_msg("expected a type");
            return self.node(SyntaxKind::Type, kids);
        }
        while self.at(SyntaxKind::Star) {
            let mut wrapped = vec![self.node(SyntaxKind::Type, kids)];
            wrapped.extend(self.bump());
            kids = wrapped;
        }
        self.node(SyntaxKind::Type, kids)
    }

    fn parse_generic_args(&mut self) -> u32 {
        let mut kids = self.expect(SyntaxKind::Lt);
        if !self.at(SyntaxKind::Gt) && !self.eof() {
            kids.push(self.parse_type());
            while self.at(SyntaxKind::Comma) {
                kids.extend(self.bump());
                if self.at(SyntaxKind::Gt) {
                    break;
                }
                kids.push(self.parse_type());
            }
        }
        kids.extend(self.expect(SyntaxKind::Gt));
        self.node(SyntaxKind::GenericArgs, kids)
    }

    fn parse_name(&mut self) -> u32 {
        if self.at(SyntaxKind::Ident) {
            let kids = self.bump();
            self.node(SyntaxKind::Name, kids)
        } else {
            self.error_msg("expected a name");
            self.node(SyntaxKind::Name, Vec::new())
        }
    }

    fn looks_like_decl(&mut self) -> bool {
        if self.out_of_class_fn() {
            return true;
        }
        if !self.type_start() {
            return false;
        }
        let cp = self.cp();
        let _ = self.parse_type();
        let ok = self.at(SyntaxKind::Ident)
            && matches!(
                self.nth(1),
                SyntaxKind::Semi | SyntaxKind::Eq | SyntaxKind::LParen | SyntaxKind::ColonColon
            );
        self.rewind(cp);
        ok
    }

    fn looks_like_for_init(&mut self) -> bool {
        if !self.type_start() {
            return false;
        }
        let cp = self.cp();
        let _ = self.parse_type();
        let ok = self.at(SyntaxKind::Ident)
            && matches!(self.nth(1), SyntaxKind::Eq | SyntaxKind::Semi);
        self.rewind(cp);
        ok
    }

    fn looks_like_range(&mut self) -> bool {
        if !self.type_start() {
            return false;
        }
        let cp = self.cp();
        let _ = self.parse_type();
        let ok = self.at(SyntaxKind::Ident)
            && matches!(self.nth(1), SyntaxKind::KwIn | SyntaxKind::Colon);
        self.rewind(cp);
        ok
    }

    fn out_of_class_fn(&self) -> bool {
        self.at(SyntaxKind::Ident)
            && self.nth(1) == SyntaxKind::ColonColon
            && self.nth(2) == SyntaxKind::Ident
            && self.nth(3) == SyntaxKind::LParen
    }

    fn generic_on_ident(&self) -> bool {
        let Some(after) = self.scan_generic_from(1) else {
            return false;
        };
        matches!(
            self.kind_at_sig(after),
            SyntaxKind::LParen | SyntaxKind::LBrace
        )
    }

    fn generic_call_from_here(&self) -> bool {
        let Some(after) = self.scan_generic_from(0) else {
            return false;
        };
        self.kind_at_sig(after) == SyntaxKind::LParen
    }

    fn scan_generic(&self) -> Option<usize> {
        self.scan_generic_from(0)
    }

    /// Index of the significant token after a closing `>`, if the `<` at
    /// significant offset `from` is balanced.
    fn scan_generic_from(&self, from: usize) -> Option<usize> {
        let mut sigs = Vec::new();
        let mut i = self.i;
        while i < self.tokens.len() {
            if !self.tokens[i].kind.is_trivia() {
                sigs.push(i);
            }
            i += 1;
        }
        if from >= sigs.len() || self.tokens[sigs[from]].kind != SyntaxKind::Lt {
            return None;
        }
        let mut depth = 0i32;
        let mut paren = 0i32;
        for (n, &ti) in sigs.iter().enumerate().skip(from) {
            match self.tokens[ti].kind {
                SyntaxKind::Lt => depth += 1,
                SyntaxKind::Gt if paren == 0 => {
                    depth -= 1;
                    if depth == 0 {
                        return Some(n + 1);
                    }
                }
                SyntaxKind::LParen => paren += 1,
                SyntaxKind::RParen => paren -= 1,
                _ => {}
            }
        }
        None
    }

    fn kind_at_sig(&self, n: usize) -> SyntaxKind {
        self.nth(n)
    }

    fn infix_bp(&self) -> Option<(u8, u8)> {
        Some(match self.nth(0) {
            SyntaxKind::QuestionQuestion => (6, 7),
            SyntaxKind::Shl => (8, 9),
            SyntaxKind::PipePipe => (10, 11),
            SyntaxKind::AmpAmp => (12, 13),
            SyntaxKind::EqEq
            | SyntaxKind::NotEq
            | SyntaxKind::Lt
            | SyntaxKind::Gt
            | SyntaxKind::LtEq
            | SyntaxKind::GtEq => (14, 15),
            SyntaxKind::Concat => (16, 17),
            SyntaxKind::Plus | SyntaxKind::Minus => (18, 19),
            SyntaxKind::Star | SyntaxKind::Slash | SyntaxKind::Percent => (20, 21),
            SyntaxKind::StarStar => (30, 30),
            _ => return None,
        })
    }

    fn is_assign_op(&self) -> bool {
        matches!(
            self.nth(0),
            SyntaxKind::Eq
                | SyntaxKind::PlusEq
                | SyntaxKind::MinusEq
                | SyntaxKind::StarEq
                | SyntaxKind::SlashEq
                | SyntaxKind::PercentEq
                | SyntaxKind::CaretEq
                | SyntaxKind::FloorDivEq
                | SyntaxKind::ConcatEq
        )
    }

    fn is_specifier(&self) -> bool {
        matches!(
            self.nth(0),
            SyntaxKind::KwConst
                | SyntaxKind::KwStatic
                | SyntaxKind::KwConstexpr
                | SyntaxKind::KwInline
                | SyntaxKind::KwAsync
                | SyntaxKind::KwOverride
        )
    }

    fn at_type_name(&self) -> bool {
        matches!(
            self.nth(0),
            SyntaxKind::Ident
                | SyntaxKind::KwVoid
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

    fn type_start(&self) -> bool {
        self.is_specifier() || self.at_type_name()
    }

    fn is_access(&self) -> bool {
        matches!(
            self.nth(0),
            SyntaxKind::KwPublic | SyntaxKind::KwPrivate | SyntaxKind::KwProtected
        )
    }

    fn at_attr(&self) -> bool {
        self.at(SyntaxKind::LBracket) && self.nth(1) == SyntaxKind::LBracket
    }

    fn is_literal(&self) -> bool {
        matches!(
            self.nth(0),
            SyntaxKind::IntLit
                | SyntaxKind::HexLit
                | SyntaxKind::FloatLit
                | SyntaxKind::StringLit
                | SyntaxKind::CharLit
                | SyntaxKind::RawString
                | SyntaxKind::KwTrue
                | SyntaxKind::KwFalse
                | SyntaxKind::KwNull
                | SyntaxKind::KwNullptr
        )
    }

    fn is_named_cast(&self) -> bool {
        self.at(SyntaxKind::Ident)
            && self.nth(1) == SyntaxKind::Lt
            && matches!(
                self.nth_text(0),
                "static_cast" | "const_cast" | "reinterpret_cast" | "dynamic_cast"
            )
    }

    fn can_start_expr(&self) -> bool {
        self.can_start_expr_after(0)
    }

    fn can_start_expr_after(&self, n: usize) -> bool {
        matches!(
            self.nth(n),
            SyntaxKind::Ident
                | SyntaxKind::IntLit
                | SyntaxKind::HexLit
                | SyntaxKind::FloatLit
                | SyntaxKind::StringLit
                | SyntaxKind::CharLit
                | SyntaxKind::RawString
                | SyntaxKind::KwTrue
                | SyntaxKind::KwFalse
                | SyntaxKind::KwNull
                | SyntaxKind::KwNullptr
                | SyntaxKind::KwAwait
                | SyntaxKind::KwNew
                | SyntaxKind::KwFunc
                | SyntaxKind::LParen
                | SyntaxKind::LBrace
                | SyntaxKind::At
                | SyntaxKind::Backtick
                | SyntaxKind::Bang
                | SyntaxKind::Minus
        )
    }

    fn at_expr_boundary(&self) -> bool {
        matches!(
            self.nth(0),
            SyntaxKind::Eof
                | SyntaxKind::Semi
                | SyntaxKind::RParen
                | SyntaxKind::RBrace
                | SyntaxKind::RBracket
                | SyntaxKind::Comma
                | SyntaxKind::Colon
                | SyntaxKind::FatArrow
                | SyntaxKind::KwElse
        )
    }

    fn recover_item(&mut self) -> u32 {
        let kids = self.recover_balanced();
        self.node(SyntaxKind::Error, kids)
    }

    fn recover_balanced(&mut self) -> Vec<u32> {
        let mut kids = Vec::new();
        let mut depth = 0i32;
        while !self.eof() {
            if depth == 0 && !kids.is_empty() && self.can_start_item() && !self.at(SyntaxKind::Ident)
            {
                break;
            }
            let k = self.nth(0);
            if matches!(k, SyntaxKind::LBrace | SyntaxKind::LParen | SyntaxKind::LBracket) {
                depth += 1;
            }
            if matches!(k, SyntaxKind::RBrace | SyntaxKind::RParen | SyntaxKind::RBracket) {
                if depth == 0 {
                    break;
                }
                depth -= 1;
            }
            kids.extend(self.bump());
            if k == SyntaxKind::Semi && depth == 0 {
                break;
            }
            if kids.len() > 4000 {
                break;
            }
        }
        if kids.is_empty() && !self.eof() {
            kids.extend(self.bump());
        }
        kids
    }

    fn expect(&mut self, kind: SyntaxKind) -> Vec<u32> {
        if self.at(kind) {
            self.bump()
        } else {
            self.error_msg(format!("expected {kind:?}"));
            vec![self.node(SyntaxKind::Error, Vec::new())]
        }
    }

    fn bump(&mut self) -> Vec<u32> {
        let mut ids = Vec::new();
        while self.i < self.tokens.len() && self.tokens[self.i].kind.is_trivia() {
            ids.push(self.push_raw());
        }
        if self.i < self.tokens.len() {
            ids.push(self.push_raw());
        }
        ids
    }

    fn push_raw(&mut self) -> u32 {
        let tok = self.tokens[self.i];
        self.i += 1;
        let id = self.elems.len() as u32;
        self.elems.push(Elem::Token {
            kind: tok.kind,
            span: tok.span,
        });
        id
    }

    fn consume_until(&mut self, end: u32) -> Vec<u32> {
        let mut ids = Vec::new();
        while self.i < self.tokens.len() && self.tokens[self.i].span.start < end {
            ids.push(self.push_raw());
        }
        ids
    }

    fn node(&mut self, kind: SyntaxKind, children: Vec<u32>) -> u32 {
        let span = self.cover(&children);
        let id = self.elems.len() as u32;
        self.elems.push(Elem::Node {
            kind,
            children,
            span,
        });
        id
    }

    fn append_children(&mut self, id: u32, suffix: Vec<u32>) {
        if suffix.is_empty() {
            return;
        }
        if let Elem::Node { children, .. } = &mut self.elems[id as usize] {
            children.extend(suffix);
        }
        let kids = match &self.elems[id as usize] {
            Elem::Node { children, .. } => children.clone(),
            Elem::Token { .. } => return,
        };
        let span = self.cover(&kids);
        if let Elem::Node { span: slot, .. } = &mut self.elems[id as usize] {
            *slot = span;
        }
    }

    fn prepend(&mut self, id: u32, mut prefix: Vec<u32>) {
        if prefix.is_empty() {
            return;
        }
        let rest = match &self.elems[id as usize] {
            Elem::Node { children, .. } => children.clone(),
            Elem::Token { .. } => return,
        };
        prefix.extend(rest);
        let span = self.cover(&prefix);
        if let Elem::Node {
            children, span: slot, ..
        } = &mut self.elems[id as usize]
        {
            *children = prefix;
            *slot = span;
        }
    }

    fn cover(&self, children: &[u32]) -> Span {
        let mut span: Option<Span> = None;
        for &c in children {
            let s = self.elem_span(c);
            span = Some(match span {
                Some(acc) => acc.cover(s),
                None => s,
            });
        }
        span.unwrap_or_else(|| Span::empty(self.here()))
    }

    fn elem_span(&self, id: u32) -> Span {
        match &self.elems[id as usize] {
            Elem::Token { span, .. } | Elem::Node { span, .. } => *span,
        }
    }

    fn cp(&self) -> Cp {
        Cp {
            i: self.i,
            elems: self.elems.len(),
            diags: self.diags.len(),
        }
    }

    fn rewind(&mut self, cp: Cp) {
        self.i = cp.i;
        self.elems.truncate(cp.elems);
        self.diags.truncate(cp.diags);
    }

    fn at(&self, kind: SyntaxKind) -> bool {
        self.nth(0) == kind
    }

    fn nth(&self, n: usize) -> SyntaxKind {
        let mut i = self.i;
        let mut seen = 0usize;
        while i < self.tokens.len() {
            if !self.tokens[i].kind.is_trivia() {
                if seen == n {
                    return self.tokens[i].kind;
                }
                seen += 1;
            }
            i += 1;
        }
        SyntaxKind::Eof
    }

    fn nth_text(&self, n: usize) -> &str {
        let mut i = self.i;
        let mut seen = 0usize;
        while i < self.tokens.len() {
            if !self.tokens[i].kind.is_trivia() {
                if seen == n {
                    let s = self.tokens[i].span;
                    return self
                        .source
                        .get(s.start as usize..s.end as usize)
                        .unwrap_or("");
                }
                seen += 1;
            }
            i += 1;
        }
        ""
    }

    fn eof(&self) -> bool {
        self.nth(0) == SyntaxKind::Eof
    }

    fn sig_index(&self) -> usize {
        let mut i = self.i;
        while i < self.tokens.len() && self.tokens[i].kind.is_trivia() {
            i += 1;
        }
        i
    }

    fn sig_span(&self) -> Option<Span> {
        let i = self.sig_index();
        self.tokens.get(i).map(|t| t.span)
    }

    fn here(&self) -> u32 {
        self.sig_span()
            .map(|s| s.start)
            .unwrap_or(self.source.len() as u32)
    }

    fn error_msg(&mut self, message: impl Into<String>) {
        let start = self.here();
        let end = self
            .sig_span()
            .map(|s| s.end)
            .unwrap_or(start)
            .max(start);
        self.diags.push(
            Diagnostic::error(Span::new(start, end), message)
                .with_code("CLPP1200"),
        );
    }
}

fn is_include_directive(rest: &str) -> bool {
    let Some(after) = rest.strip_prefix("include") else {
        return false;
    };
    after.is_empty()
        || after.starts_with(|c: char| c.is_whitespace() || c == '"' || c == '<')
}
