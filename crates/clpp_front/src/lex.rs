//! Hand-written lexer.
//!
//! `//` starts a line comment unless the third character is `=`. The sequence
//! `//=` is floor-division assignment, which the old PEG never reached because
//! a `//` comment consumed it first. `///` and `//!` stay comments.
//!
//! Every byte of the file is covered by exactly one token, including
//! whitespace and comments. Bad input is an `Error` token plus a diagnostic,
//! not a stopped lexer.

use crate::diag::Diagnostic;
use crate::kind::SyntaxKind;
use crate::span::Span;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Token {
    pub kind: SyntaxKind,
    pub span: Span,
}

#[derive(Clone, Debug)]
pub struct Lexed {
    pub tokens: Vec<Token>,
    pub diagnostics: Vec<Diagnostic>,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Mode {
    Normal,
    Template,
}

pub fn lex(source: &str) -> Lexed {
    let mut lx = Lexer {
        source,
        i: 0,
        mode: Mode::Normal,
        interp: 0,
        diagnostics: Vec::new(),
    };
    let mut tokens = Vec::new();
    while lx.i < source.len() {
        let before = lx.i;
        if let Some(tok) = lx.next_token() {
            if tok.span.end > tok.span.start {
                tokens.push(tok);
            }
        }
        if lx.i == before {
            // Never stall on a byte we do not understand.
            let ch = source[lx.i..].chars().next().unwrap_or('\u{FFFD}');
            let end = lx.i + ch.len_utf8();
            lx.diagnostics.push(unknown_char(lx.i, end, ch));
            tokens.push(Token {
                kind: SyntaxKind::Error,
                span: Span::new(lx.i as u32, end as u32),
            });
            lx.i = end;
        }
    }
    debug_assert!(tokens_cover(source, &tokens));
    Lexed {
        tokens,
        diagnostics: lx.diagnostics,
    }
}

fn tokens_cover(source: &str, tokens: &[Token]) -> bool {
    let mut at = 0u32;
    for t in tokens {
        if t.span.start != at {
            return false;
        }
        at = t.span.end;
    }
    at as usize == source.len()
}

struct Lexer<'a> {
    source: &'a str,
    i: usize,
    mode: Mode,
    interp: u32,
    diagnostics: Vec<Diagnostic>,
}

impl<'a> Lexer<'a> {
    fn rest(&self) -> &'a str {
        &self.source[self.i..]
    }

    fn peek(&self) -> Option<char> {
        self.rest().chars().next()
    }

    fn bump(&mut self) -> Option<char> {
        let ch = self.peek()?;
        self.i += ch.len_utf8();
        Some(ch)
    }

    fn tok(&self, kind: SyntaxKind, start: usize) -> Token {
        Token {
            kind,
            span: Span::new(start as u32, self.i as u32),
        }
    }

    fn next_token(&mut self) -> Option<Token> {
        if self.mode == Mode::Template {
            return self.lex_template();
        }
        let start = self.i;
        let ch = self.peek()?;
        if ch == '\u{feff}' || ch.is_whitespace() {
            while self.peek().is_some_and(|c| c == '\u{feff}' || c.is_whitespace()) {
                self.bump();
            }
            return Some(self.tok(SyntaxKind::Whitespace, start));
        }
        if self.rest().starts_with("//") {
            if self.rest().as_bytes().get(2) == Some(&b'=') {
                self.i += 3;
                return Some(self.tok(SyntaxKind::FloorDivEq, start));
            }
            self.i += 2;
            while self.peek().is_some_and(|c| c != '\n') {
                self.bump();
            }
            return Some(self.tok(SyntaxKind::LineComment, start));
        }
        if self.rest().starts_with("/*") {
            self.i += 2;
            if let Some(rel) = self.rest().find("*/") {
                self.i += rel + 2;
            } else {
                self.i = self.source.len();
                self.diagnostics.push(
                    Diagnostic::error(
                        Span::new(start as u32, self.i as u32),
                        "unterminated block comment",
                    )
                    .with_code("CLPP1201"),
                );
            }
            return Some(self.tok(SyntaxKind::BlockComment, start));
        }
        if self.rest().starts_with("R\"(") {
            return Some(self.lex_raw(start));
        }
        match ch {
            '"' | '\'' => Some(self.lex_quoted(start, ch)),
            '`' => {
                self.i += 1;
                self.mode = Mode::Template;
                Some(self.tok(SyntaxKind::Backtick, start))
            }
            c if c.is_ascii_digit() => Some(self.lex_number(start)),
            c if is_ident_start(c) => Some(self.lex_ident(start)),
            '{' if self.interp > 0 => {
                self.interp += 1;
                self.i += 1;
                Some(self.tok(SyntaxKind::LBrace, start))
            }
            '}' if self.interp > 0 => {
                self.interp -= 1;
                self.i += 1;
                if self.interp == 0 {
                    self.mode = Mode::Template;
                }
                Some(self.tok(SyntaxKind::RBrace, start))
            }
            _ => self.lex_punct(start),
        }
    }

    fn lex_template(&mut self) -> Option<Token> {
        let start = self.i;
        if self.rest().starts_with('`') {
            self.i += 1;
            self.mode = Mode::Normal;
            return Some(self.tok(SyntaxKind::Backtick, start));
        }
        if self.rest().starts_with('{') {
            self.i += 1;
            self.mode = Mode::Normal;
            self.interp = 1;
            return Some(self.tok(SyntaxKind::LBrace, start));
        }
        if self.i >= self.source.len() {
            return None;
        }
        while self.i < self.source.len() {
            if self.rest().starts_with('`') || self.rest().starts_with('{') {
                break;
            }
            if self.rest().starts_with('\\') {
                self.i += 1;
                if self.peek().is_none() {
                    break;
                }
                self.bump();
                continue;
            }
            self.bump();
        }
        if self.i == start {
            self.diagnostics.push(
                Diagnostic::error(Span::new(start as u32, self.i as u32), "unterminated template string")
                    .with_code("CLPP1201"),
            );
            return None;
        }
        if self.i >= self.source.len() {
            self.diagnostics.push(
                Diagnostic::error(
                    Span::new(start as u32, self.i as u32),
                    "unterminated template string",
                )
                .with_code("CLPP1201"),
            );
        }
        Some(self.tok(SyntaxKind::TemplateText, start))
    }

    fn lex_raw(&mut self, start: usize) -> Token {
        self.i += 3; // R"(
        if let Some(rel) = self.rest().find(")\"") {
            self.i += rel + 2;
        } else {
            self.i = self.source.len();
            self.diagnostics.push(
                Diagnostic::error(Span::new(start as u32, self.i as u32), "unterminated raw string")
                    .with_code("CLPP1201"),
            );
        }
        self.tok(SyntaxKind::RawString, start)
    }

    fn lex_quoted(&mut self, start: usize, quote: char) -> Token {
        self.bump();
        let mut closed = false;
        while let Some(c) = self.peek() {
            if c == quote {
                self.bump();
                closed = true;
                break;
            }
            if c == '\\' {
                self.bump();
                match self.peek() {
                    None => break,
                    Some(esc) => {
                        if !is_simple_escape(esc) && esc != 'x' && esc != 'u' {
                            let at = self.i as u32;
                            self.diagnostics.push(
                                Diagnostic::error(
                                    Span::new(at, at + esc.len_utf8() as u32),
                                    format!("unknown string escape '\\{esc}'"),
                                )
                                .with_code("CLPP1201"),
                            );
                        }
                        self.bump();
                        if esc == 'x' {
                            self.bump_hex_digits(2);
                        } else if esc == 'u' {
                            if self.peek() == Some('{') {
                                self.bump();
                                while self.peek().is_some_and(|h| h.is_ascii_hexdigit()) {
                                    self.bump();
                                }
                                if self.peek() == Some('}') {
                                    self.bump();
                                }
                            }
                        }
                    }
                }
                continue;
            }
            if c == '\n' {
                break;
            }
            self.bump();
        }
        if !closed {
            self.diagnostics.push(
                Diagnostic::error(
                    Span::new(start as u32, self.i as u32),
                    "unterminated string",
                )
                .with_code("CLPP1201"),
            );
        }
        let kind = if quote == '"' {
            SyntaxKind::StringLit
        } else {
            SyntaxKind::CharLit
        };
        self.tok(kind, start)
    }

    fn bump_hex_digits(&mut self, n: usize) {
        for _ in 0..n {
            if self.peek().is_some_and(|c| c.is_ascii_hexdigit()) {
                self.bump();
            }
        }
    }

    fn lex_number(&mut self, start: usize) -> Token {
        if self.rest().starts_with("0x") || self.rest().starts_with("0X") {
            self.i += 2;
            let digits = self.i;
            while self.peek().is_some_and(|c| c.is_ascii_hexdigit()) {
                self.bump();
            }
            if self.i == digits {
                self.diagnostics.push(
                    Diagnostic::error(
                        Span::new(start as u32, self.i as u32),
                        "hex literal has no digits",
                    )
                    .with_code("CLPP1201"),
                );
            }
            return self.tok(SyntaxKind::HexLit, start);
        }
        while self.peek().is_some_and(|c| c.is_ascii_digit()) {
            self.bump();
        }
        let mut is_float = false;
        if self.peek() == Some('.')
            && self.rest().chars().nth(1).is_some_and(|c| c.is_ascii_digit())
        {
            is_float = true;
            self.bump();
            while self.peek().is_some_and(|c| c.is_ascii_digit()) {
                self.bump();
            }
        }
        if matches!(self.peek(), Some('e') | Some('E')) {
            let save = self.i;
            self.bump();
            if matches!(self.peek(), Some('+') | Some('-')) {
                self.bump();
            }
            let exp = self.i;
            while self.peek().is_some_and(|c| c.is_ascii_digit()) {
                self.bump();
            }
            if self.i == exp {
                self.i = save;
                self.diagnostics.push(
                    Diagnostic::error(
                        Span::new(start as u32, (save + 1) as u32),
                        "exponent has no digits",
                    )
                    .with_code("CLPP1201"),
                );
            } else {
                is_float = true;
            }
        }
        self.tok(
            if is_float {
                SyntaxKind::FloatLit
            } else {
                SyntaxKind::IntLit
            },
            start,
        )
    }

    fn lex_ident(&mut self, start: usize) -> Token {
        self.bump();
        while self.peek().is_some_and(is_ident_continue) {
            self.bump();
        }
        let text = &self.source[start..self.i];
        self.tok(keyword(text), start)
    }

    fn lex_punct(&mut self, start: usize) -> Option<Token> {
        let rest = self.rest();
        let pairs: &[(&str, SyntaxKind)] = &[
            (".:=", SyntaxKind::ConcatEq),
            ("..=", SyntaxKind::Error), // not an operator; fall through by not matching below
            ("?.", SyntaxKind::QuestionDot),
            ("??", SyntaxKind::QuestionQuestion),
            ("::", SyntaxKind::ColonColon),
            (".:", SyntaxKind::Concat),
            ("..<", SyntaxKind::RangeExclusive),
            ("..", SyntaxKind::Range),
            ("=>", SyntaxKind::FatArrow),
            ("++", SyntaxKind::PlusPlus),
            ("--", SyntaxKind::MinusMinus),
            ("**", SyntaxKind::StarStar),
            ("<<", SyntaxKind::Shl),
            ("&&", SyntaxKind::AmpAmp),
            ("||", SyntaxKind::PipePipe),
            ("==", SyntaxKind::EqEq),
            ("!=", SyntaxKind::NotEq),
            ("<=", SyntaxKind::LtEq),
            (">=", SyntaxKind::GtEq),
            ("+=", SyntaxKind::PlusEq),
            ("-=", SyntaxKind::MinusEq),
            ("*=", SyntaxKind::StarEq),
            ("/=", SyntaxKind::SlashEq),
            ("%=", SyntaxKind::PercentEq),
            ("^=", SyntaxKind::CaretEq),
        ];
        for (text, kind) in pairs {
            if *kind == SyntaxKind::Error {
                continue;
            }
            if rest.starts_with(text) {
                self.i += text.len();
                return Some(self.tok(*kind, start));
            }
        }
        let kind = match self.bump()? {
            '+' => SyntaxKind::Plus,
            '-' => SyntaxKind::Minus,
            '*' => SyntaxKind::Star,
            '/' => SyntaxKind::Slash,
            '%' => SyntaxKind::Percent,
            '^' => SyntaxKind::Caret,
            '!' => SyntaxKind::Bang,
            '&' => SyntaxKind::Amp,
            '|' => SyntaxKind::Pipe,
            '<' => SyntaxKind::Lt,
            '>' => SyntaxKind::Gt,
            '=' => SyntaxKind::Eq,
            ':' => SyntaxKind::Colon,
            ';' => SyntaxKind::Semi,
            ',' => SyntaxKind::Comma,
            '.' => SyntaxKind::Dot,
            '?' => SyntaxKind::Question,
            '@' => SyntaxKind::At,
            '#' => SyntaxKind::Hash,
            '(' => SyntaxKind::LParen,
            ')' => SyntaxKind::RParen,
            '{' => SyntaxKind::LBrace,
            '}' => SyntaxKind::RBrace,
            '[' => SyntaxKind::LBracket,
            ']' => SyntaxKind::RBracket,
            other => {
                self.diagnostics
                    .push(unknown_char(start, self.i, other));
                SyntaxKind::Error
            }
        };
        Some(self.tok(kind, start))
    }
}

fn unknown_char(start: usize, end: usize, ch: char) -> Diagnostic {
    Diagnostic::error(
        Span::new(start as u32, end as u32),
        format!("unknown character '{ch}'"),
    )
    .with_code("CLPP1201")
}

fn is_simple_escape(c: char) -> bool {
    matches!(c, 'n' | 'r' | 't' | '0' | '\\' | '\'' | '"' | '`')
}

fn is_ident_start(c: char) -> bool {
    c.is_ascii_alphabetic() || c == '_'
}

fn is_ident_continue(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_'
}

fn keyword(text: &str) -> SyntaxKind {
    match text {
        "async" => SyntaxKind::KwAsync,
        "await" => SyntaxKind::KwAwait,
        "as" => SyntaxKind::KwAs,
        "auto" => SyntaxKind::KwAuto,
        "bool" => SyntaxKind::KwBool,
        "break" => SyntaxKind::KwBreak,
        "by" => SyntaxKind::KwBy,
        "case" => SyntaxKind::KwCase,
        "catch" => SyntaxKind::KwCatch,
        "class" => SyntaxKind::KwClass,
        "comptime" => SyntaxKind::KwComptime,
        "const" => SyntaxKind::KwConst,
        "constexpr" => SyntaxKind::KwConstexpr,
        "continue" => SyntaxKind::KwContinue,
        "default" => SyntaxKind::KwDefault,
        "do" => SyntaxKind::KwDo,
        "double" => SyntaxKind::KwDouble,
        "else" => SyntaxKind::KwElse,
        "enum" => SyntaxKind::KwEnum,
        "extern" => SyntaxKind::KwExtern,
        "false" => SyntaxKind::KwFalse,
        "float" => SyntaxKind::KwFloat,
        "for" => SyntaxKind::KwFor,
        "func" => SyntaxKind::KwFunc,
        "guard" => SyntaxKind::KwGuard,
        "if" => SyntaxKind::KwIf,
        "import" => SyntaxKind::KwImport,
        "in" => SyntaxKind::KwIn,
        "inline" => SyntaxKind::KwInline,
        "int" => SyntaxKind::KwInt,
        "interface" => SyntaxKind::KwInterface,
        "link" => SyntaxKind::KwLink,
        "match" => SyntaxKind::KwMatch,
        "namespace" => SyntaxKind::KwNamespace,
        "new" => SyntaxKind::KwNew,
        "null" => SyntaxKind::KwNull,
        "nullptr" => SyntaxKind::KwNullptr,
        "override" => SyntaxKind::KwOverride,
        "private" => SyntaxKind::KwPrivate,
        "protected" => SyntaxKind::KwProtected,
        "public" => SyntaxKind::KwPublic,
        "return" => SyntaxKind::KwReturn,
        "static" => SyntaxKind::KwStatic,
        "string" => SyntaxKind::KwString,
        "struct" => SyntaxKind::KwStruct,
        "switch" => SyntaxKind::KwSwitch,
        "template" => SyntaxKind::KwTemplate,
        "true" => SyntaxKind::KwTrue,
        "try" => SyntaxKind::KwTry,
        "tuple" => SyntaxKind::KwTuple,
        "type" => SyntaxKind::KwType,
        "typedef" => SyntaxKind::KwTypedef,
        "typename" => SyntaxKind::KwTypename,
        "using" => SyntaxKind::KwUsing,
        "variant" => SyntaxKind::KwVariant,
        "void" => SyntaxKind::KwVoid,
        "while" => SyntaxKind::KwWhile,
        _ => SyntaxKind::Ident,
    }
}
