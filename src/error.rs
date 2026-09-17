use miette::{Diagnostic, SourceSpan};
use thiserror::Error;

#[derive(Debug, Error, Diagnostic)]
#[error("{message}")]
#[diagnostic(code(clpp::error))]
pub struct ClppError {
    #[source_code]
    pub src: String,
    #[label("{label}")]
    pub span: SourceSpan,
    pub message: String,
    pub label: String,
}

impl ClppError {
    pub fn new(src: impl Into<String>, span: SourceSpan, message: impl Into<String>) -> Self {
        let message = message.into();
        Self {
            src: src.into(),
            span,
            label: message.clone(),
            message,
        }
    }

    pub fn at_line(src: impl Into<String>, line: usize, col: usize, message: impl Into<String>) -> Self {
        let src = src.into();
        let span = line_col_span(&src, line, col);
        Self::new(src, span, message)
    }

    pub fn line_col(&self) -> (usize, usize) {
        offset_to_line_col(&self.src, self.span.offset())
    }
}

pub fn offset_to_line_col(src: &str, offset: usize) -> (usize, usize) {
    let mut line = 1usize;
    let mut col = 1usize;
    for (i, ch) in src.char_indices() {
        if i >= offset {
            break;
        }
        if ch == '\n' {
            line += 1;
            col = 1;
        } else {
            col += 1;
        }
    }
    (line, col)
}

pub fn line_col_span(src: &str, line: usize, col: usize) -> SourceSpan {
    let mut pos = 0usize;
    for (i, row) in src.split_inclusive('\n').enumerate() {
        if i + 1 == line {
            let offset = col.saturating_sub(1);
            let len = 1.max(row.len().saturating_sub(offset));
            return (pos + offset, len.min(row.len())).into();
        }
        pos += row.len();
    }
    (src.len().saturating_sub(1), 1).into()
}
