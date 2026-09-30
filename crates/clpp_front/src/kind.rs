//! Token and syntax-node kinds for the phase-1 front end.
//!
//! Kinds are one `u16` space, the same way rust-analyzer shares tokens and
//! nodes in a rowan tree. Trivia stays in the tree so a later formatter or
//! language server can see comments without re-lexing.

macro_rules! kinds {
    ($($name:ident),* $(,)?) => {
        #[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
        #[repr(u16)]
        pub enum SyntaxKind {
            $($name),*
        }

        impl SyntaxKind {
            pub fn from_raw(raw: u16) -> SyntaxKind {
                const ALL: &[SyntaxKind] = &[$(SyntaxKind::$name),*];
                ALL.get(raw as usize).copied().unwrap_or(SyntaxKind::Error)
            }

            pub fn is_trivia(self) -> bool {
                matches!(self, SyntaxKind::Whitespace | SyntaxKind::LineComment | SyntaxKind::BlockComment)
            }
        }
    };
}

kinds! {
    Whitespace,
    LineComment,
    BlockComment,
    Error,
    Eof,
    Ident,
    IntLit,
    HexLit,
    FloatLit,
    StringLit,
    CharLit,
    RawString,
    TemplateText,
    KwAsync,
    KwAwait,
    KwAs,
    KwAuto,
    KwBool,
    KwBreak,
    KwBy,
    KwCase,
    KwCatch,
    KwClass,
    KwComptime,
    KwConst,
    KwConstexpr,
    KwContinue,
    KwDefault,
    KwDo,
    KwDouble,
    KwElse,
    KwEnum,
    KwExtern,
    KwFalse,
    KwFloat,
    KwFor,
    KwFunc,
    KwGuard,
    KwIf,
    KwImport,
    KwIn,
    KwInline,
    KwInt,
    KwInterface,
    KwLink,
    KwMatch,
    KwNamespace,
    KwNew,
    KwNull,
    KwNullptr,
    KwOverride,
    KwPrivate,
    KwProtected,
    KwPublic,
    KwReturn,
    KwStatic,
    KwString,
    KwStruct,
    KwSwitch,
    KwTemplate,
    KwTrue,
    KwTry,
    KwTuple,
    KwType,
    KwTypedef,
    KwTypename,
    KwUsing,
    KwVariant,
    KwVoid,
    KwWhile,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Caret,
    Bang,
    Amp,
    Pipe,
    Lt,
    Gt,
    Eq,
    Colon,
    Semi,
    Comma,
    Dot,
    Question,
    At,
    Hash,
    Backtick,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    PlusEq,
    MinusEq,
    StarEq,
    SlashEq,
    PercentEq,
    CaretEq,
    FloorDivEq,
    ConcatEq,
    EqEq,
    NotEq,
    LtEq,
    GtEq,
    PlusPlus,
    MinusMinus,
    AmpAmp,
    PipePipe,
    QuestionQuestion,
    QuestionDot,
    ColonColon,
    Concat,
    FatArrow,
    Range,
    RangeExclusive,
    Shl,
    StarStar,
    SourceFile,
    Function,
    Struct,
    Class,
    Interface,
    Enum,
    EnumVariant,
    Namespace,
    TypeAlias,
    Var,
    Link,
    RejectedModule,
    Directive,
    Attribute,
    TemplateHead,
    TemplateParam,
    ParamList,
    Param,
    Type,
    GenericArgs,
    Name,
    Block,
    If,
    Guard,
    Match,
    MatchArm,
    Switch,
    While,
    DoWhile,
    For,
    Return,
    Break,
    Continue,
    ExprStmt,
    Empty,
    Comptime,
    Destructure,
    Literal,
    NameRef,
    Binary,
    Unary,
    Call,
    Member,
    Index,
    Assign,
    Ternary,
    Try,
    Cast,
    Lambda,
    Init,
    AtExpr,
    Paren,
    NamedCast,
    New,
    Await,
    Template,
    StringJoin,
    ArgList,
    RangeExpr,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Lang;

impl rowan::Language for Lang {
    type Kind = SyntaxKind;

    fn kind_from_raw(raw: rowan::SyntaxKind) -> Self::Kind {
        SyntaxKind::from_raw(raw.0)
    }

    fn kind_to_raw(kind: Self::Kind) -> rowan::SyntaxKind {
        rowan::SyntaxKind(kind as u16)
    }
}

pub type SyntaxNode = rowan::SyntaxNode<Lang>;
pub type SyntaxToken = rowan::SyntaxToken<Lang>;

#[cfg(test)]
mod tests {
    use super::SyntaxKind;

    #[test]
    fn discriminants_round_trip() {
        for raw in 0..SyntaxKind::RangeExpr as u16 + 1 {
            let kind = SyntaxKind::from_raw(raw);
            assert_eq!(kind as u16, raw, "{kind:?}");
        }
    }
}
