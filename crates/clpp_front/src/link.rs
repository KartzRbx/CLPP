//! Local name of a `link` target.
//!
//! One function owns the rule: the alias if `as` is present, otherwise the
//! file stem with its source extension removed. A package link (`@a.b.c`)
//! uses the last segment and is never turned into a filesystem path.

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum LinkTarget {
    /// Quoted path such as `./shared/Wallet.clp`.
    Path(String),
    /// `@segment.segment` package reference. Segments never include slashes.
    Package(Vec<String>),
}

impl LinkTarget {
    pub fn display(&self) -> String {
        match self {
            LinkTarget::Path(path) => format!("\"{path}\""),
            LinkTarget::Package(segs) => format!("@{}", segs.join(".")),
        }
    }
}

/// Binding introduced by one `link`.
///
/// `as_written` means the source has an `as` clause. A present clause never
/// falls back to the stem: a missing or non-identifier alias binds nothing.
/// Without `as`, the stem or last package segment is the binding only when
/// it is an identifier (`Wallet`, not `a.b` or `Main.role`).
pub fn link_binding_name(target: &LinkTarget, alias: Option<&str>, as_written: bool) -> Option<String> {
    if as_written {
        return alias.map(str::trim).filter(|s| is_binding_ident(s)).map(str::to_string);
    }
    let raw = match target {
        LinkTarget::Path(path) => file_stem(path),
        LinkTarget::Package(segs) => segs.last().cloned().unwrap_or_default(),
    };
    if is_binding_ident(&raw) {
        Some(raw)
    } else {
        None
    }
}

/// ASCII identifier, the same set the lexer accepts.
pub fn is_binding_ident(name: &str) -> bool {
    let mut chars = name.chars();
    match chars.next() {
        Some(c) if c.is_ascii_alphabetic() || c == '_' => chars.all(|c| c.is_ascii_alphanumeric() || c == '_'),
        _ => false,
    }
}

/// Last path component with a trailing `.clpp`, `.clp`, or `.clh` removed.
///
/// `Wallet.clp` is `Wallet`, not `clp`. A dotted remainder such as `a.b` or
/// `Main.role` is returned as-is; [`link_binding_name`] then rejects it.
pub fn file_stem(path: &str) -> String {
    let path = path.trim().trim_matches('"').replace('\\', "/");
    let base = path.rsplit('/').next().unwrap_or(path.as_str());
    let lower = base.to_ascii_lowercase();
    for ext in [".clpp", ".clp", ".clh"] {
        if lower.ends_with(ext) && base.len() > ext.len() {
            return base[..base.len() - ext.len()].to_string();
        }
    }
    if base.is_empty() {
        "module".into()
    } else {
        base.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn alias_wins() {
        let t = LinkTarget::Path("./shared/Wallet.clp".into());
        assert_eq!(link_binding_name(&t, Some("Purse"), true).as_deref(), Some("Purse"));
    }

    #[test]
    fn missing_alias_does_not_fall_back_to_the_stem() {
        let t = LinkTarget::Path("./shared/Wallet.clp".into());
        assert_eq!(link_binding_name(&t, None, true), None);
        assert_eq!(link_binding_name(&t, Some("1"), true), None);
        assert_eq!(link_binding_name(&t, Some(""), true), None);
    }

    #[test]
    fn stem_strips_extension_after_the_directory() {
        let t = LinkTarget::Path("./shared/Wallet.clp".into());
        assert_eq!(link_binding_name(&t, None, false).as_deref(), Some("Wallet"));
        assert_eq!(file_stem("../PlayerData.clh"), "PlayerData");
        assert_eq!(file_stem("A.B.clpp"), "A.B");
        assert_eq!(link_binding_name(&LinkTarget::Path("./a.b.clp".into()), None, false), None);
        assert_eq!(link_binding_name(&LinkTarget::Path(".clh".into()), None, false), None);
        assert_eq!(
            link_binding_name(&LinkTarget::Path("Main.server.clpp".into()), None, false),
            None
        );
    }

    #[test]
    fn package_uses_last_segment_and_is_not_a_path() {
        let t = LinkTarget::Package(vec!["pkg".into(), "libs".into(), "kit".into()]);
        assert_eq!(link_binding_name(&t, None, false).as_deref(), Some("kit"));
        assert_eq!(link_binding_name(&t, Some("Alias"), true).as_deref(), Some("Alias"));
        let shown = t.display();
        assert_eq!(shown, "@pkg.libs.kit");
        assert!(!shown.contains('/'));
        assert!(!link_binding_name(&t, None, false).unwrap().contains('/'));
    }
}
