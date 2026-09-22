#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Span {
    pub start_line: usize,
    pub start_col: usize,
    pub end_line: usize,
    pub end_col: usize,
}

impl Span {
    pub fn new(start_line: usize, start_col: usize, end_line: usize, end_col: usize) -> Self {
        Self {
            start_line: start_line.max(1),
            start_col: start_col.max(1),
            end_line: end_line.max(1),
            end_col: end_col.max(1),
        }
    }

    pub fn point(line: usize, col: usize) -> Self {
        let line = line.max(1);
        let col = col.max(1);
        Self::new(line, col, line, col)
    }

    pub fn contains(&self, line: usize, col: usize) -> bool {
        if line < self.start_line || line > self.end_line {
            return false;
        }
        if line == self.start_line && col < self.start_col {
            return false;
        }
        if line == self.end_line && col > self.end_col {
            return false;
        }
        true
    }

    pub fn contains_line(&self, line: usize) -> bool {
        line >= self.start_line && line <= self.end_line
    }

    /// Expand to cover both spans (ignores default/empty).
    pub fn cover(self, other: Span) -> Span {
        if other.start_line == 0 {
            return self;
        }
        if self.start_line == 0 {
            return other;
        }
        Span {
            start_line: self.start_line.min(other.start_line),
            start_col: if self.start_line <= other.start_line {
                self.start_col
            } else {
                other.start_col
            },
            end_line: self.end_line.max(other.end_line),
            end_col: if self.end_line >= other.end_line {
                self.end_col
            } else {
                other.end_col
            },
        }
    }
}

#[derive(Debug, Clone)]
pub struct SourceComment {
    pub line: usize,
    pub text: String,
    pub is_doc: bool,
}
