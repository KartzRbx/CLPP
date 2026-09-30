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

pub fn link_binding_name(target: &LinkTarget, alias: Option<&str>) -> String {
    if let Some(alias) = alias.map(str::trim).filter(|s| !s.is_empty()) {
        return alias.to_string();
    }
    match target {
        LinkTarget::Path(path) => file_stem(path),
        LinkTarget::Package(segs) => segs
            .last()
            .filter(|s| !s.is_empty())
            .cloned()
            .unwrap_or_else(|| "module".into()),
    }
}

/// Last path component with a trailing `.clpp`, `.clp`, or `.clh` removed.
///
/// `Wallet.clp` is `Wallet`, not `clp`. A role suffix such as `.server` is
/// left in place (`Main.server.clpp` → `Main.server`); whether that should
/// become an identifier is an open question for a later phase.
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
        assert_eq!(link_binding_name(&t, Some("Purse")), "Purse");
    }

    #[test]
    fn stem_strips_extension_after_the_directory() {
        let t = LinkTarget::Path("./shared/Wallet.clp".into());
        assert_eq!(link_binding_name(&t, None), "Wallet");
        assert_eq!(file_stem("../PlayerData.clh"), "PlayerData");
        assert_eq!(file_stem("A.B.clpp"), "A.B");
    }

    #[test]
    fn package_uses_last_segment_and_is_not_a_path() {
        let t = LinkTarget::Package(vec!["pkg".into(), "libs".into(), "tool".into()]);
        assert_eq!(link_binding_name(&t, None), "tool");
        assert_eq!(link_binding_name(&t, Some("Alias")), "Alias");
        let shown = t.display();
        assert_eq!(shown, "@pkg.libs.tool");
        assert!(!shown.contains('/'));
        assert!(!link_binding_name(&t, None).contains('/'));
    }
}
