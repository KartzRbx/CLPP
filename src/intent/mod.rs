//! Intent System surface (RFC 0008 Phase A / B).
//!
//! - RFC 0009: comptime + reflection
//! - RFC 0010: checked generics
//! - RFC 0012: Option / Result constructors + `?`
//! - RFC 0013: rich match on Option/Result tags
//! - RFC 0014–0018: reserved keywords / hooks (contracts, effects, typestate, newtype, move)
//! - RFC 0019: Phase B deferred

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

/// Reserved Intent keywords (RFCs 0014–0018). Recognized by docs/LSP; grammar hooks later.
pub const INTENT_RESERVED: &[&str] = &[
    "requires",
    "ensures",
    "effect",
    "pure",
    "newtype",
    "unique",
    "move",
];

/// Effect tags for RFC 0015 lite (metadata only until checker lands).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EffectTag {
    Pure,
    Io,
    Yields,
    Mutates,
}

pub fn parse_effect_tag(s: &str) -> Option<EffectTag> {
    match s {
        "pure" => Some(EffectTag::Pure),
        "io" => Some(EffectTag::Io),
        "yields" => Some(EffectTag::Yields),
        "mutates" => Some(EffectTag::Mutates),
        _ => None,
    }
}

/// Typestate tag placeholder (RFC 0016).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TypeState {
    pub type_name: String,
    pub state: String,
}

/// Nominal distinct wrapper (RFC 0017) — registry entry for future checker.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Newtype {
    pub name: String,
    pub underlying: String,
}
