//! Concrete syntax kinds for CL++.
//!
//! `link` is the only module form. `@clpp` and `@game` are nodes
//! (`At` + ident). `Error` keeps a bad span so an incomplete file still has a tree.

use rowan::Language;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(u16)]
pub enum SyntaxKind {
    Whitespace = 0,
    Comment,
    LinkKw,
    AsKw,
    FromKw,
    LetKw,
    ImportKw,
    Ident,
    At,
    Dot,
    Hash,
    Eq,
    Plus,
    Minus,
    Star,
    Slash,
    Bang,
    Amp,
    Pipe,
    Lt,
    Gt,
    Question,
    Colon,
    Comma,
    Semi,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    StringLit,
    IntLit,
    SourceFile,
    LinkStmt,
    /// `@clpp` path head (standard library).
    AtClpp,
    /// `@game` path head (DataModel / Rojo).
    AtGame,
    Path,
    LetStmt,
    Expr,
    /// Unexpected or missing syntax. The prefix already parsed stays in the tree.
    Error,
    Eof,
    Tombstone,
}

const KINDS: &[SyntaxKind] = &[
    SyntaxKind::Whitespace,
    SyntaxKind::Comment,
    SyntaxKind::LinkKw,
    SyntaxKind::AsKw,
    SyntaxKind::FromKw,
    SyntaxKind::LetKw,
    SyntaxKind::ImportKw,
    SyntaxKind::Ident,
    SyntaxKind::At,
    SyntaxKind::Dot,
    SyntaxKind::Hash,
    SyntaxKind::Eq,
    SyntaxKind::Plus,
    SyntaxKind::Minus,
    SyntaxKind::Star,
    SyntaxKind::Slash,
    SyntaxKind::Bang,
    SyntaxKind::Amp,
    SyntaxKind::Pipe,
    SyntaxKind::Lt,
    SyntaxKind::Gt,
    SyntaxKind::Question,
    SyntaxKind::Colon,
    SyntaxKind::Comma,
    SyntaxKind::Semi,
    SyntaxKind::LParen,
    SyntaxKind::RParen,
    SyntaxKind::LBrace,
    SyntaxKind::RBrace,
    SyntaxKind::LBracket,
    SyntaxKind::RBracket,
    SyntaxKind::StringLit,
    SyntaxKind::IntLit,
    SyntaxKind::SourceFile,
    SyntaxKind::LinkStmt,
    SyntaxKind::AtClpp,
    SyntaxKind::AtGame,
    SyntaxKind::Path,
    SyntaxKind::LetStmt,
    SyntaxKind::Expr,
    SyntaxKind::Error,
    SyntaxKind::Eof,
    SyntaxKind::Tombstone,
];

impl SyntaxKind {
    pub fn from_u16(raw: u16) -> Option<Self> {
        KINDS.get(raw as usize).copied()
    }

    pub fn is_trivia(self) -> bool {
        matches!(self, Self::Whitespace | Self::Comment)
    }
}

impl From<SyntaxKind> for rowan::SyntaxKind {
    fn from(kind: SyntaxKind) -> Self {
        Self(kind as u16)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum ClppLanguage {}

impl Language for ClppLanguage {
    type Kind = SyntaxKind;

    fn kind_from_raw(raw: rowan::SyntaxKind) -> Self::Kind {
        SyntaxKind::from_u16(raw.0).unwrap_or(SyntaxKind::Error)
    }

    fn kind_to_raw(kind: Self::Kind) -> rowan::SyntaxKind {
        kind.into()
    }
}

pub type SyntaxNode = rowan::SyntaxNode<ClppLanguage>;
pub type SyntaxToken = rowan::SyntaxToken<ClppLanguage>;
pub type SyntaxElement = rowan::SyntaxElement<ClppLanguage>;
pub type GreenNode = rowan::GreenNode;
pub type GreenToken = rowan::GreenToken;
pub type GreenNodeBuilder<'a> = rowan::GreenNodeBuilder<'a>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn kind_discriminants_match_table() {
        for (i, kind) in KINDS.iter().copied().enumerate() {
            assert_eq!(kind as u16, i as u16);
            assert_eq!(SyntaxKind::from_u16(i as u16), Some(kind));
        }
    }
}
