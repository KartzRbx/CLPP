//! Language service facade. Protocol: [`tower_lsp_server`]. Semantics: [`crate::analysis`] / Session.
//! Does not emit Luau.

pub mod server;

pub use crate::analysis as service;
pub use server::run;
