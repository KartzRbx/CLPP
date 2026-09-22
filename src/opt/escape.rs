//! Escape-aware helpers for scalar replacement (RFC 0011 Phase 1 lite).

use crate::ast::Expr;
use crate::names::is_library_type;

/// Datatypes whose `.new(...)` + field reads can become scalars when they do not escape.
pub fn vector_fields(class_name: &str) -> Option<&'static [&'static str]> {
    match class_name {
        "Vector2" | "Vector2int16" => Some(&["X", "Y"]),
        "Vector3" | "Vector3int16" => Some(&["X", "Y", "Z"]),
        _ => None,
    }
}

pub fn is_scalarizable_new(class_name: &str) -> bool {
    vector_fields(class_name).is_some() && is_library_type(class_name)
}

/// Map positional `Type.new(a,b,…)` args onto field names.
pub fn zip_vector_args(class_name: &str, args: &[Expr]) -> Option<Vec<(String, Expr)>> {
    let fields = vector_fields(class_name)?;
    if args.len() < fields.len() {
        return None;
    }
    Some(
        fields
            .iter()
            .zip(args.iter())
            .map(|(f, a)| ((*f).to_string(), a.clone()))
            .collect(),
    )
}
