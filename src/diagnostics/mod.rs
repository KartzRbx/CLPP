//! Syntax vs semantic diagnostic codes. The language service reads these; it does not parse Pest itself.

pub use crate::diag::{all as codes, diag, explain, message, warning, Code};
