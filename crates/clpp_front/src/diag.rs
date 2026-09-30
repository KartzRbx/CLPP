//! Diagnostics with byte spans, rendered as `file:line:col`.
//!
//! The short form is stable for tests. The CLI also prints a codespan report
//! in the style of the rustc diagnostic guide (primary label, source line,
//! help note). A parse never stops at the first error: callers collect the
//! whole vector.

use crate::span::{LineIndex, Span};
use codespan_reporting::diagnostic::{Label, Severity as CsSeverity};
use codespan_reporting::files::SimpleFile;
use codespan_reporting::term;
use codespan_reporting::term::termcolor::{ColorChoice, NoColor, StandardStream};
use std::io::Write;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Severity {
    Error,
    Warning,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Diagnostic {
    pub severity: Severity,
    pub message: String,
    pub span: Span,
    pub code: Option<String>,
    pub help: Option<String>,
}

impl Diagnostic {
    pub fn error(span: Span, message: impl Into<String>) -> Self {
        Self {
            severity: Severity::Error,
            message: message.into(),
            span,
            code: None,
            help: None,
        }
    }

    pub fn with_code(mut self, code: impl Into<String>) -> Self {
        self.code = Some(code.into());
        self
    }

    pub fn with_help(mut self, help: impl Into<String>) -> Self {
        self.help = Some(help.into());
        self
    }

    /// Fields a future `clpp api compile` diagnostic object can copy.
    /// `line` and `column` are 1-based. `end_line` / `end_column` are there
    /// so a span-based source map can grow past the line-only JSON used today.
    pub fn api_view(&self, source: &str) -> ApiDiagnostic {
        let lines = LineIndex::new(source);
        let (line, column) = lines.line_col_in(source, self.span.start);
        let (end_line, end_column) = lines.line_col_in(source, self.span.end);
        ApiDiagnostic {
            message: self.message.clone(),
            line,
            column,
            end_line,
            end_column,
            severity: match self.severity {
                Severity::Error => "error",
                Severity::Warning => "warning",
            }
            .into(),
            code: self.code.clone(),
            help: self.help.clone(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ApiDiagnostic {
    pub message: String,
    pub line: u32,
    pub column: u32,
    pub end_line: u32,
    pub end_column: u32,
    pub severity: String,
    pub code: Option<String>,
    pub help: Option<String>,
}

pub fn render(file_name: &str, source: &str, diags: &[Diagnostic]) -> String {
    let lines = LineIndex::new(source);
    let mut out = String::new();
    for (i, d) in diags.iter().enumerate() {
        if i > 0 {
            out.push('\n');
        }
        render_one(&mut out, file_name, source, &lines, d);
    }
    out
}

fn render_one(out: &mut String, file_name: &str, source: &str, lines: &LineIndex, d: &Diagnostic) {
    let sev = match d.severity {
        Severity::Error => "error",
        Severity::Warning => "warning",
    };
    let (line, col) = lines.line_col_in(source, d.span.start);
    if let Some(code) = &d.code {
        out.push_str(&format!("{sev}[{code}]: {}\n", d.message));
    } else {
        out.push_str(&format!("{sev}: {}\n", d.message));
    }
    out.push_str(&format!("  --> {file_name}:{line}:{col}\n"));
    let text = lines.line_text(source, line);
    let num = line.to_string();
    let pad = " ".repeat(num.len());
    out.push_str(&format!(" {pad} |\n"));
    out.push_str(&format!(" {num} | {text}\n"));
    let start_col = col as usize;
    let (end_line, end_col) = lines.line_col_in(source, d.span.end);
    let mark = if end_line == line && end_col > col {
        (end_col - col) as usize
    } else if d.span.len() == 0 {
        1
    } else {
        1
    };
    let mark = mark.max(1);
    out.push_str(&format!(
        " {pad} | {blank}{caret}\n",
        blank = " ".repeat(start_col.saturating_sub(1)),
        caret = "^".repeat(mark.min(text.chars().count().saturating_sub(start_col.saturating_sub(1)).max(1)).max(1)),
    ));
    if let Some(help) = &d.help {
        out.push_str(&format!(" {pad} |\n"));
        out.push_str(&format!(" {pad} = help: {help}\n"));
    }
}

/// codespan report written to a string (no color). Used by the CLI and tests
/// that want the rustc-style layout from the diagnostic guide.
pub fn render_codespan(file_name: &str, source: &str, diags: &[Diagnostic]) -> String {
    let file = SimpleFile::new(file_name, source);
    let config = term::Config::default();
    let mut buf = NoColor::new(Vec::new());
    for d in diags {
        let sev = match d.severity {
            Severity::Error => CsSeverity::Error,
            Severity::Warning => CsSeverity::Warning,
        };
        let range = d.span.start as usize..d.span.end.max(d.span.start) as usize;
        let range = if range.start > source.len() {
            source.len()..source.len()
        } else {
            range.start.min(source.len())..range.end.min(source.len()).max(range.start.min(source.len()))
        };
        let mut report = codespan_reporting::diagnostic::Diagnostic::new(sev)
            .with_message(d.message.clone())
            .with_labels(vec![Label::primary((), range).with_message(d.message.clone())]);
        if let Some(code) = &d.code {
            report = report.with_code(code);
        }
        if let Some(help) = &d.help {
            report = report.with_notes(vec![help.clone()]);
        }
        let _ = term::emit(&mut buf, &config, &file, &report);
    }
    String::from_utf8(buf.into_inner()).unwrap_or_default()
}

pub fn print_codespan(file_name: &str, source: &str, diags: &[Diagnostic]) {
    let rendered = render_codespan(file_name, source, diags);
    let mut stderr = StandardStream::stderr(ColorChoice::Never);
    let _ = write!(stderr, "{rendered}");
}
