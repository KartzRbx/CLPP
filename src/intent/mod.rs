//! Intent System surface (RFC 0008 Phase A).
//!
//! - RFC 0009: comptime + reflection
//! - RFC 0010: checked generics
//! - RFC 0012: Option / Result constructors

/// Builtins only valid inside `comptime { ... }`.
pub fn is_reflect_builtin(name: &str) -> bool {
    matches!(
        name,
        "field_names" | "field_count" | "type_name" | "has_field"
    )
}

/// Option / Result constructors (RFC 0012).
pub fn is_option_result_builtin(name: &str) -> bool {
    matches!(name, "Some" | "None" | "Ok" | "Err")
}

/// All Intent reflection builtins (for docs / LSP).
pub const REFLECT_BUILTINS: &[&str] = &["field_names", "field_count", "type_name", "has_field"];

pub const OPTION_RESULT_BUILTINS: &[&str] = &["Some", "None", "Ok", "Err"];
