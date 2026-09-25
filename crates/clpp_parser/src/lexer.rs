use clpp_syntax::SyntaxKind;

pub struct Lexer<'a> {
    text: &'a str,
    pos: usize,
}

impl<'a> Lexer<'a> {
    pub fn new(text: &'a str) -> Self {
        Self { text, pos: 0 }
    }

    fn rest(&self) -> &'a str {
        &self.text[self.pos..]
    }

    fn bump(&mut self, n: usize) -> &'a str {
        let start = self.pos;
        self.pos += n;
        &self.text[start..self.pos]
    }

    fn next_token(&mut self) -> Option<(SyntaxKind, &'a str)> {
        let rest = self.rest();
        let mut chars = rest.chars();
        let c = chars.next()?;
        let kind = match c {
            c if c.is_whitespace() => {
                let n = rest
                    .chars()
                    .take_while(|ch| ch.is_whitespace())
                    .map(|ch| ch.len_utf8())
                    .sum();
                return Some((SyntaxKind::Whitespace, self.bump(n)));
            }
            '/' if rest.starts_with("//") => {
                let n = rest.find('\n').unwrap_or(rest.len());
                return Some((SyntaxKind::Comment, self.bump(n)));
            }
            '#' => SyntaxKind::Hash,
            '@' => SyntaxKind::At,
            '.' => SyntaxKind::Dot,
            '=' => SyntaxKind::Eq,
            '+' => SyntaxKind::Plus,
            '-' => SyntaxKind::Minus,
            '*' => SyntaxKind::Star,
            '/' => SyntaxKind::Slash,
            '!' => SyntaxKind::Bang,
            '&' => SyntaxKind::Amp,
            '|' => SyntaxKind::Pipe,
            '<' => SyntaxKind::Lt,
            '>' => SyntaxKind::Gt,
            '?' => SyntaxKind::Question,
            ':' => SyntaxKind::Colon,
            ',' => SyntaxKind::Comma,
            ';' => SyntaxKind::Semi,
            '(' => SyntaxKind::LParen,
            ')' => SyntaxKind::RParen,
            '{' => SyntaxKind::LBrace,
            '}' => SyntaxKind::RBrace,
            '[' => SyntaxKind::LBracket,
            ']' => SyntaxKind::RBracket,
            '"' => {
                let mut n = 1;
                for ch in rest[1..].chars() {
                    n += ch.len_utf8();
                    if ch == '"' {
                        break;
                    }
                }
                return Some((SyntaxKind::StringLit, self.bump(n)));
            }
            c if c.is_ascii_digit() => {
                let n = rest
                    .chars()
                    .take_while(|ch| ch.is_ascii_digit())
                    .map(|ch| ch.len_utf8())
                    .sum();
                return Some((SyntaxKind::IntLit, self.bump(n)));
            }
            c if is_ident_start(c) => {
                let n = rest
                    .chars()
                    .take_while(|ch| is_ident_continue(*ch))
                    .map(|ch| ch.len_utf8())
                    .sum();
                let text = self.bump(n);
                return Some((keyword(text), text));
            }
            _ => {
                return Some((SyntaxKind::Error, self.bump(c.len_utf8())));
            }
        };
        Some((kind, self.bump(c.len_utf8())))
    }
}

impl<'a> Iterator for Lexer<'a> {
    type Item = (SyntaxKind, &'a str);

    fn next(&mut self) -> Option<Self::Item> {
        self.next_token()
    }
}

fn keyword(text: &str) -> SyntaxKind {
    match text {
        "link" => SyntaxKind::LinkKw,
        "as" => SyntaxKind::AsKw,
        "from" => SyntaxKind::FromKw,
        "let" => SyntaxKind::LetKw,
        "import" => SyntaxKind::ImportKw,
        _ => SyntaxKind::Ident,
    }
}

fn is_ident_start(c: char) -> bool {
    c.is_ascii_alphabetic() || c == '_'
}

fn is_ident_continue(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_'
}
