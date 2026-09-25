//! Symbol index and completion over the rowan CST.
//! The process `clpp lsp` still serves the root crate; this is the new index.

mod complete;
mod lsp_diag;

pub use complete::{complete, CompletionItem};
pub use index::SymbolIndex;
pub use lsp_diag::diagnostic_to_lsp;

mod index;
