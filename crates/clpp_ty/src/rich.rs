use miette::{Diagnostic, NamedSource, SourceSpan};
use thiserror::Error;

use crate::{self as ty, RunContext};

#[derive(Error, Debug, Diagnostic)]
pub enum ClppDiagnostic {
    #[error("Context Safety violation (CLUAU_AUTH)")]
    #[diagnostic(
        code(CLUAU_AUTH001),
        help("a Client context cannot call functions marked @server")
    )]
    ContextSafety {
        #[source_code]
        src: NamedSource<String>,
        #[label("authority attribute or call in the wrong run context")]
        span: SourceSpan,
        detail: ty::Diagnostic,
    },

    #[error("Thread Safety violation (CLUAU-PAR)")]
    #[diagnostic(
        code(CLUAU_PAR001),
        help("do not assign shared memory inside @parallel")
    )]
    ParallelSafety {
        #[source_code]
        src: NamedSource<String>,
        #[label("mutation is not allowed on a parallel thread")]
        span: SourceSpan,
        detail: ty::Diagnostic,
    },
}

pub fn rich_diagnostics(file: &str, source: &str, ctx: RunContext) -> Vec<ClppDiagnostic> {
    crate::check(source, ctx)
        .diagnostics
        .into_iter()
        .map(|detail| {
            let span = SourceSpan::from((detail.start, detail.len));
            let src = NamedSource::new(file, source.to_string());
            if detail.code.starts_with("CLUAU_PAR") {
                ClppDiagnostic::ParallelSafety { src, span, detail }
            } else {
                ClppDiagnostic::ContextSafety { src, span, detail }
            }
        })
        .collect()
}
