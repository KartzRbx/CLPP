use clpp_ty::Diagnostic as TyDiag;
use lsp_types::{Diagnostic, DiagnosticSeverity, NumberOrString, Range};

/// Byte span → LSP range. `Error` nodes are irrelevant here; the span is the source line.
pub fn diagnostic_to_lsp(diag: &TyDiag, text: &str) -> Diagnostic {
    Diagnostic {
        range: span_to_range(diag.start, diag.len, text),
        severity: Some(DiagnosticSeverity::ERROR),
        code: Some(NumberOrString::String(diag.code.to_string())),
        code_description: None,
        source: Some("clpp-analyzer".into()),
        message: diag.message.clone(),
        related_information: None,
        tags: None,
        data: None,
    }
}

pub fn span_to_range(start: usize, len: usize, text: &str) -> Range {
    let end = start.saturating_add(len).min(text.len());
    Range {
        start: offset_to_pos(text, start),
        end: offset_to_pos(text, end),
    }
}

fn offset_to_pos(text: &str, offset: usize) -> lsp_types::Position {
    let offset = offset.min(text.len());
    let mut line = 0u32;
    let mut col = 0u32;
    for (i, b) in text.bytes().enumerate() {
        if i >= offset {
            break;
        }
        if b == b'\n' {
            line += 1;
            col = 0;
        } else {
            col += 1;
        }
    }
    lsp_types::Position { line, character: col }
}

#[cfg(test)]
mod tests {
    use super::*;
    use clpp_ty::{check, RunContext};

    #[test]
    fn auth_span_becomes_lsp_range() {
        let src = "@server\nvoid Save();\nvoid init() {\n    Save();\n}\n";
        let diag = check(src, RunContext::Client)
            .diagnostics
            .into_iter()
            .find(|d| d.message.contains("Save"))
            .unwrap();
        let lsp = diagnostic_to_lsp(&diag, src);
        assert_eq!(lsp.range.start.line, 3);
        match lsp.code {
            Some(lsp_types::NumberOrString::String(code)) => assert_eq!(code, "CLUAU_AUTH001"),
            other => panic!("{other:?}"),
        }
        assert_eq!(lsp.source.as_deref(), Some("clpp-analyzer"));
    }
}
