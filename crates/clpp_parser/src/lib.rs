//! Error-tolerant parser for the `link` module form.
//!
//! `import` and `#include` parse as an `Error` node plus a fixed diagnostic.
//! A truncated statement (`let x =`) still finishes the tree.

mod lexer;

use clpp_syntax::{GreenNode, GreenNodeBuilder, SyntaxKind, SyntaxNode};
use lexer::Lexer;
use rowan::TextRange;

pub const USE_LINK: &str = "use `link`";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParseError {
    pub message: String,
    pub range: TextRange,
}

#[derive(Debug)]
pub struct Parse {
    pub green: GreenNode,
    pub errors: Vec<ParseError>,
}

impl Parse {
    pub fn syntax(&self) -> SyntaxNode {
        SyntaxNode::new_root(self.green.clone())
    }
}

pub fn parse(text: &str) -> Parse {
    Parser::new(text).parse_source()
}

struct Parser<'a> {
    tokens: Vec<(SyntaxKind, &'a str)>,
    pos: usize,
    builder: GreenNodeBuilder<'static>,
    errors: Vec<ParseError>,
    offset: usize,
}

impl<'a> Parser<'a> {
    fn new(text: &'a str) -> Self {
        Self {
            tokens: Lexer::new(text).collect(),
            pos: 0,
            builder: GreenNodeBuilder::new(),
            errors: Vec::new(),
            offset: 0,
        }
    }

    fn parse_source(mut self) -> Parse {
        self.builder.start_node(SyntaxKind::SourceFile.into());
        while !self.eof() {
            self.skip_trivia();
            if self.eof() {
                break;
            }
            match self.nth_kind(0) {
                SyntaxKind::LinkKw => self.parse_link(),
                SyntaxKind::ImportKw => self.reject_old_module(false),
                SyntaxKind::Hash => self.reject_old_module(true),
                SyntaxKind::LetKw => self.parse_let(),
                _ => self.recover_unexpected(),
            }
        }
        self.builder.finish_node();
        Parse {
            green: self.builder.finish(),
            errors: self.errors,
        }
    }

    fn parse_link(&mut self) {
        self.builder.start_node(SyntaxKind::LinkStmt.into());
        self.bump();
        self.skip_trivia();
        if self.at_path_start() {
            self.parse_path();
        } else {
            self.error_here("expected a path after `link`");
            self.recover_until_stmt();
        }
        self.skip_trivia();
        if self.nth_kind(0) == SyntaxKind::AsKw {
            self.bump();
            self.skip_trivia();
            if self.nth_kind(0) == SyntaxKind::Ident {
                self.bump();
            } else {
                self.error_here("expected an alias after `as`");
                self.recover_until_stmt();
            }
        }
        self.skip_trivia();
        if self.nth_kind(0) == SyntaxKind::Semi {
            self.bump();
        } else {
            self.error_here("expected `;`");
        }
        self.builder.finish_node();
    }

    fn parse_path(&mut self) {
        self.builder.start_node(SyntaxKind::Path.into());
        if self.nth_kind(0) == SyntaxKind::StringLit {
            self.bump();
            self.builder.finish_node();
            return;
        }
        let head = self.ident_after_at();
        let head_kind = match head {
            Some("clpp") => SyntaxKind::AtClpp,
            Some("game") => SyntaxKind::AtGame,
            _ => SyntaxKind::Error,
        };
        if !matches!(head_kind, SyntaxKind::AtClpp | SyntaxKind::AtGame) && head.is_some() {
            self.error_here("expected `@clpp` or `@game`");
        }
        self.builder.start_node(head_kind.into());
        self.bump();
        self.skip_trivia();
        if self.nth_kind(0) == SyntaxKind::Ident {
            self.bump();
        } else {
            self.error_here("expected a name after `@`");
        }
        self.builder.finish_node();
        loop {
            self.skip_trivia();
            if self.nth_kind(0) != SyntaxKind::Dot {
                break;
            }
            self.bump();
            self.skip_trivia();
            if self.nth_kind(0) == SyntaxKind::Ident {
                self.bump();
            } else {
                self.error_here("expected a name after `.`");
                break;
            }
        }
        self.builder.finish_node();
    }

    fn parse_let(&mut self) {
        self.builder.start_node(SyntaxKind::LetStmt.into());
        self.bump();
        self.skip_trivia();
        if self.nth_kind(0) == SyntaxKind::Ident {
            self.bump();
        } else {
            self.error_here("expected a name");
        }
        self.skip_trivia();
        if self.nth_kind(0) == SyntaxKind::Eq {
            self.bump();
            self.skip_trivia();
            self.parse_expr();
        }
        self.skip_trivia();
        if self.nth_kind(0) == SyntaxKind::Semi {
            self.bump();
        }
        self.builder.finish_node();
    }

    /// Expression or an `Error` node when the right-hand side was cut off (`let x =`).
    fn parse_expr(&mut self) {
        if self.expr_start() {
            self.builder.start_node(SyntaxKind::Expr.into());
            self.bump();
            self.builder.finish_node();
            return;
        }
        let start = self.offset();
        self.builder.start_node(SyntaxKind::Error.into());
        self.recover_until_stmt();
        self.builder.finish_node();
        self.errors.push(ParseError {
            message: "expected expression".into(),
            range: TextRange::new(start.try_into().unwrap_or_default(), self.offset().try_into().unwrap_or_default()),
        });
    }

    fn reject_old_module(&mut self, hash_include: bool) {
        if hash_include {
            let include = self.nth_kind(self.next_non_trivia(1)) == SyntaxKind::Ident
                && self.nth_text(self.next_non_trivia(1)) == Some("include");
            if !include {
                self.recover_unexpected();
                return;
            }
        }
        let start = self.offset();
        self.builder.start_node(SyntaxKind::Error.into());
        self.bump_until_stmt();
        self.builder.finish_node();
        self.errors.push(ParseError {
            message: USE_LINK.into(),
            range: TextRange::new(start.try_into().unwrap_or_default(), self.offset().try_into().unwrap_or_default()),
        });
    }

    fn recover_unexpected(&mut self) {
        let start = self.offset();
        self.builder.start_node(SyntaxKind::Error.into());
        self.bump_until_stmt();
        self.builder.finish_node();
        self.errors.push(ParseError {
            message: "unexpected token".into(),
            range: TextRange::new(start.try_into().unwrap_or_default(), self.offset().try_into().unwrap_or_default()),
        });
    }

    fn recover_until_stmt(&mut self) {
        self.builder.start_node(SyntaxKind::Error.into());
        while !self.eof() && !self.at_sync() {
            self.bump();
        }
        self.builder.finish_node();
    }

    fn bump_until_stmt(&mut self) {
        if self.eof() {
            return;
        }
        self.bump();
        while !self.eof() && !self.at_sync() {
            self.bump();
        }
        if self.nth_kind(0) == SyntaxKind::Semi {
            self.bump();
        }
    }

    fn at_sync(&self) -> bool {
        matches!(
            self.nth_kind(0),
            SyntaxKind::Semi | SyntaxKind::RBrace | SyntaxKind::RParen | SyntaxKind::Eof
        )
    }

    fn at_path_start(&self) -> bool {
        matches!(self.nth_kind(0), SyntaxKind::At | SyntaxKind::StringLit)
    }

    fn expr_start(&self) -> bool {
        matches!(
            self.nth_kind(0),
            SyntaxKind::Ident | SyntaxKind::StringLit | SyntaxKind::IntLit | SyntaxKind::LParen
        )
    }

    fn ident_after_at(&self) -> Option<&'a str> {
        if self.nth_kind(0) != SyntaxKind::At {
            return None;
        }
        let i = self.next_non_trivia(1);
        if self.nth_kind(i) == SyntaxKind::Ident {
            self.nth_text(i)
        } else {
            None
        }
    }

    fn next_non_trivia(&self, mut from: usize) -> usize {
        while self.nth_kind(from).is_trivia() {
            from += 1;
        }
        from
    }

    fn eof(&self) -> bool {
        self.pos >= self.tokens.len()
    }

    fn nth_kind(&self, n: usize) -> SyntaxKind {
        self.tokens.get(self.pos + n).map(|(k, _)| *k).unwrap_or(SyntaxKind::Eof)
    }

    fn nth_text(&self, n: usize) -> Option<&'a str> {
        self.tokens.get(self.pos + n).map(|(_, t)| *t)
    }

    fn offset(&self) -> usize {
        self.offset
    }

    fn skip_trivia(&mut self) {
        while self.nth_kind(0).is_trivia() {
            self.bump();
        }
    }

    fn bump(&mut self) {
        if let Some((kind, text)) = self.tokens.get(self.pos).copied() {
            self.builder.token(kind.into(), text);
            self.offset += text.len();
            self.pos += 1;
        }
    }

    fn error_here(&mut self, message: &str) {
        let at = self.offset();
        self.errors.push(ParseError {
            message: message.into(),
            range: TextRange::new(at.try_into().unwrap_or_default(), at.try_into().unwrap_or_default()),
        });
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use clpp_syntax::SyntaxKind;

    fn child_kinds(node: &clpp_syntax::SyntaxNode) -> Vec<SyntaxKind> {
        node.children().map(|n| n.kind()).collect()
    }

    #[test]
    fn parses_clpp_link_with_alias() {
        let parsed = parse("link @clpp.libs.janitor as Janitor;");
        assert!(parsed.errors.is_empty(), "{:?}", parsed.errors);
        let root = parsed.syntax();
        let link = root.children().next().expect("link stmt");
        assert_eq!(link.kind(), SyntaxKind::LinkStmt);
        assert!(link.to_string().contains("@clpp"));
        assert!(child_kinds(&link).contains(&SyntaxKind::Path));
        let path = link.children().find(|n| n.kind() == SyntaxKind::Path).unwrap();
        assert!(child_kinds(&path).contains(&SyntaxKind::AtClpp));
    }

    #[test]
    fn incomplete_let_is_an_error_node() {
        let parsed = parse("let x = ");
        assert!(parsed.errors.iter().any(|e| e.message == "expected expression"));
        let root = parsed.syntax();
        let stmt = root.children().next().unwrap();
        assert_eq!(stmt.kind(), SyntaxKind::LetStmt);
        assert!(child_kinds(&stmt).contains(&SyntaxKind::Error));
    }

    #[test]
    fn import_and_include_say_use_link() {
        for src in ["import { Janitor } from \"./x.clh\";", "#include <clpp/roblox.clh>"] {
            let parsed = parse(src);
            assert!(
                parsed.errors.iter().any(|e| e.message == USE_LINK),
                "{src}: {:?}",
                parsed.errors
            );
            let root = parsed.syntax();
            assert_eq!(root.children().next().unwrap().kind(), SyntaxKind::Error);
        }
    }
}
