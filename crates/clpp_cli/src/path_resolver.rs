//! Resolve `link` paths: `@clpp`, `@game` (Rojo), and `./` relatives.
//!
//! The live `clpp` binary still lives in the root crate. This module is the
//! resolver the new CLI and the LSP share.

use serde_json::Value;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ResolveError {
    Unsupported,
    MissingStdlib(PathBuf),
    MissingProject,
    MissingInstance(String),
    Io(String),
}

impl std::fmt::Display for ResolveError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Unsupported => write!(f, "not a link path"),
            Self::MissingStdlib(p) => write!(f, "stdlib file not found: {}", p.display()),
            Self::MissingProject => write!(f, "default.project.json not found"),
            Self::MissingInstance(s) => write!(f, "Rojo tree has no `{s}`"),
            Self::Io(s) => write!(f, "{s}"),
        }
    }
}

/// `@clpp` root. `CLPP_INCLUDE` wins; otherwise the installed pack, then `~/.clpp/include`.
pub fn stdlib_dir() -> PathBuf {
    if let Ok(dir) = std::env::var("CLPP_INCLUDE") {
        return PathBuf::from(dir);
    }
    if let Ok(local) = std::env::var("LOCALAPPDATA") {
        let pack = PathBuf::from(local).join("Programs").join("CLPP").join("include");
        if pack.is_dir() {
            return pack;
        }
    }
    let home = std::env::var("USERPROFILE")
        .or_else(|_| std::env::var("HOME"))
        .unwrap_or_else(|_| ".".into());
    PathBuf::from(home).join(".clpp").join("include")
}

/// `spec` is the path text from a `link` (`@clpp.libs.janitor`, `@game.ReplicatedStorage.Modules.Combat`, `./Components/Health`).
pub fn resolve_link(spec: &str, from_file: &Path, project_root: &Path) -> Result<PathBuf, ResolveError> {
    let spec = spec.trim().trim_matches('"');
    if let Some(rest) = spec.strip_prefix("@clpp.") {
        let path = names_to_file(&stdlib_dir(), rest);
        if path.is_file() {
            Ok(path)
        } else {
            Err(ResolveError::MissingStdlib(path))
        }
    } else if let Some(rest) = spec.strip_prefix("@game.") {
        resolve_game(rest, project_root)
    } else if spec.starts_with("./") || spec.starts_with("../") {
        let base = from_file.parent().unwrap_or(Path::new("."));
        let joined = base.join(spec);
        Ok(existing_source(joined))
    } else {
        Err(ResolveError::Unsupported)
    }
}

fn resolve_game(rest: &str, project_root: &Path) -> Result<PathBuf, ResolveError> {
    let project = project_root.join("default.project.json");
    let text = fs::read_to_string(&project).map_err(|_| ResolveError::MissingProject)?;
    let value: Value = serde_json::from_str(&text).map_err(|e| ResolveError::Io(e.to_string()))?;
    let mut node = value.get("tree").ok_or(ResolveError::MissingProject)?;
    let segments: Vec<&str> = rest.split('.').filter(|s| !s.is_empty()).collect();
    for (i, seg) in segments.iter().enumerate() {
        let child = node.get(*seg).ok_or_else(|| ResolveError::MissingInstance((*seg).into()))?;
        let last = i + 1 == segments.len();
        if last {
            if let Some(p) = child.get("$path").and_then(|p| p.as_str()) {
                return Ok(existing_source(project_root.join(p)));
            }
            return Err(ResolveError::MissingInstance((*seg).into()));
        }
        if let Some(p) = child.get("$path").and_then(|p| p.as_str()) {
            let rel = segments[i + 1..].join("/");
            return Ok(existing_source(project_root.join(p).join(rel)));
        }
        node = child;
    }
    Err(ResolveError::MissingInstance(rest.into()))
}

fn names_to_file(root: &Path, dotted: &str) -> PathBuf {
    let rel = dotted.split('.').collect::<Vec<_>>().join("/");
    existing_source(root.join(rel))
}

fn existing_source(path: PathBuf) -> PathBuf {
    if path.is_file() {
        return path;
    }
    for ext in ["clh", "clpp", "clp"] {
        let candidate = path.with_extension(ext);
        if candidate.is_file() {
            return candidate;
        }
    }
    path.with_extension("clh")
}

/// `clpp.toml` `[places]` entry, or `default.project.json` when the place is missing.
pub fn place_project(root: &Path, place: &str) -> PathBuf {
    let manifest = root.join("clpp.toml");
    if let Ok(text) = fs::read_to_string(&manifest) {
        let mut in_places = false;
        for line in text.lines() {
            let t = line.trim();
            if t.starts_with('[') {
                in_places = t == "[places]";
                continue;
            }
            if !in_places {
                continue;
            }
            let Some((key, value)) = t.split_once('=') else { continue };
            if key.trim() == place {
                let value = value.trim().trim_matches('"');
                return root.join(value);
            }
        }
    }
    root.join("default.project.json")
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn scratch() -> PathBuf {
        let dir = std::env::temp_dir().join(format!("clpp-resolve-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(dir.join("include").join("libs")).unwrap();
        fs::create_dir_all(dir.join("game").join("src").join("ReplicatedStorage").join("Modules")).unwrap();
        dir
    }

    #[test]
    fn resolves_clpp_game_and_relative() {
        let dir = scratch();
        let include = dir.join("include");
        fs::write(include.join("libs").join("janitor.clh"), "struct Janitor {}\n").unwrap();
        let game = dir.join("game");
        fs::write(
            game.join("default.project.json"),
            r#"{"tree":{"$className":"DataModel","ReplicatedStorage":{"$path":"src/ReplicatedStorage"}}}"#,
        )
        .unwrap();
        fs::write(
            game.join("src").join("ReplicatedStorage").join("Modules").join("Combat.clh"),
            "struct Combat {}\n",
        )
        .unwrap();
        let from = game.join("src").join("Boot.clpp");
        fs::write(&from, "").unwrap();
        fs::create_dir_all(game.join("src").join("Components")).unwrap();
        fs::write(game.join("src").join("Components").join("Health.clh"), "struct Health {}\n").unwrap();

        let prev = std::env::var("CLPP_INCLUDE").ok();
        std::env::set_var("CLPP_INCLUDE", &include);
        let janitor = resolve_link("@clpp.libs.janitor", &from, &game).unwrap();
        let combat = resolve_link("@game.ReplicatedStorage.Modules.Combat", &from, &game).unwrap();
        let health = resolve_link("./Components/Health", &from, &game).unwrap();
        match prev {
            Some(v) => std::env::set_var("CLPP_INCLUDE", v),
            None => std::env::remove_var("CLPP_INCLUDE"),
        }
        assert!(janitor.ends_with("janitor.clh"));
        assert!(combat.ends_with("Combat.clh"));
        assert!(health.ends_with("Health.clh"));
        let _ = fs::remove_dir_all(&dir);
    }
}
