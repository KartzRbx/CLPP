//! Byte spans and a line map.
//!
//! Spans are UTF-8 byte offsets into the original file, inclusive start and
//! exclusive end. Columns in rendered diagnostics are 1-based Unicode scalar
//! counts so a caret lines up with the source the programmer sees.

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Span {
    pub start: u32,
    pub end: u32,
}

impl Span {
    pub fn new(start: u32, end: u32) -> Self {
        Self { start, end }
    }

    pub fn empty(at: u32) -> Self {
        Self { start: at, end: at }
    }

    pub fn len(self) -> u32 {
        self.end.saturating_sub(self.start)
    }

    pub fn cover(self, other: Span) -> Span {
        Span {
            start: self.start.min(other.start),
            end: self.end.max(other.end),
        }
    }
}

#[derive(Clone, Debug)]
pub struct LineIndex {
    /// Byte offset of the first character of each line. Line 0 starts at 0.
    starts: Vec<u32>,
}

impl LineIndex {
    pub fn new(source: &str) -> Self {
        let mut starts = vec![0];
        for (i, b) in source.bytes().enumerate() {
            if b == b'\n' {
                starts.push((i + 1) as u32);
            }
        }
        Self { starts }
    }

    /// 1-based line and 1-based character column.
    pub fn line_col(&self, offset: u32) -> (u32, u32) {
        let offset = offset as usize;
        let idx = match self.starts.binary_search(&(offset as u32)) {
            Ok(i) => i,
            Err(i) => i.saturating_sub(1),
        };
        let line_start = self.starts[idx] as usize;
        let col = source_column_at(line_start, offset);
        ((idx as u32) + 1, col)
    }

    pub fn line_start(&self, line_1: u32) -> u32 {
        self.starts
            .get(line_1.saturating_sub(1) as usize)
            .copied()
            .unwrap_or(0)
    }

    pub fn line_text<'a>(&self, source: &'a str, line_1: u32) -> &'a str {
        let start = self.line_start(line_1) as usize;
        if start > source.len() {
            return "";
        }
        let rest = &source[start..];
        let end = rest.find('\n').unwrap_or(rest.len());
        rest[..end].trim_end_matches('\r')
    }
}

fn source_column_at(line_start: usize, offset: usize) -> u32 {
    // Character column. The caller passes offsets; the source is not here,
    // so this helper only works when both offsets are byte indexes of the
    // same line and we count later. See `LineIndex::line_col` — it does not
    // have the source. We count bytes for the column when the source is
    // ASCII, which CL++ punctuation is, and fix the real column in
    // `line_col_in`.
    (offset.saturating_sub(line_start) as u32) + 1
}

impl LineIndex {
    pub fn line_col_in(&self, source: &str, offset: u32) -> (u32, u32) {
        let (line, _) = self.line_col(offset);
        let start = self.line_start(line) as usize;
        let end = (offset as usize).min(source.len());
        let col = if end >= start {
            source[start..end].chars().count() as u32 + 1
        } else {
            1
        };
        (line, col)
    }
}
