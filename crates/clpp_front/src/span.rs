//! Byte spans and a line map.
//!
//! Spans are UTF-8 byte offsets into the original file, inclusive start and
//! exclusive end. Columns are 1-based counts of Unicode scalar values, so a
//! caret lines up with the source the programmer sees. `é` is one column,
//! even though it is two bytes. Converting those columns to LSP's UTF-16
//! code units is a later concern; this crate does not do it.

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

    /// 1-based line, and a 1-based column counted in Unicode scalar values.
    pub fn line_col(&self, source: &str, offset: u32) -> (u32, u32) {
        let offset = offset as usize;
        let idx = match self.starts.binary_search(&(offset as u32)) {
            Ok(i) => i,
            Err(i) => i.saturating_sub(1),
        };
        let line = (idx as u32) + 1;
        let start = self.starts[idx] as usize;
        let end = offset.min(source.len());
        let col = if end >= start {
            source[start..end].chars().count() as u32 + 1
        } else {
            1
        };
        (line, col)
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

impl LineIndex {
    /// Same column convention as [`LineIndex::line_col`].
    pub fn line_col_in(&self, source: &str, offset: u32) -> (u32, u32) {
        self.line_col(source, offset)
    }
}
