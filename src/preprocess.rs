use crate::ast::{CompileContext, ModuleRequire};
use crate::error::ClppError;
use crate::semantic::lib_from_include;
use miette::Result;
use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};

pub fn preprocess(source: &str, file_path: &Path) -> Result<(String, CompileContext)> {
    let mut seen = HashSet::new();
    let mut ctx = CompileContext {
        is_script: is_script(file_path),
        is_header: is_header(file_path),
        script_kind: script_kind(file_path),
        ..CompileContext::default()
    };
    let expanded = expand(source, file_path, &mut ctx, &mut seen)?;
    Ok((expanded, ctx))
}

fn expand(
    source: &str,
    file_path: &Path,
    ctx: &mut CompileContext,
    seen: &mut HashSet<PathBuf>,
) -> Result<String> {
    let key = canonicalize_or(file_path);
    if !seen.insert(key) {
        return Ok(String::new());
    }
    let stem = file_stem_name(file_path);
    let dir = file_path.parent().unwrap_or_else(|| Path::new("."));
    let mut out = String::new();
    for line in source.lines() {
        let trimmed = line.trim();
        if let Some(rest) = trimmed.strip_prefix("#include") {
            let rest = rest.trim();
            if let Some(path) = angled(rest) {
                if let Some(lib) = lib_from_include(&path) {
                    if !ctx.libraries.contains(&lib) {
                        ctx.libraries.push(lib);
                    }
                }
                continue;
            }
            if let Some(path) = quoted(rest) {
                let resolved = dir.join(&path);
                if !resolved.exists() {
                    return Err(ClppError::at_line(
                        source,
                        1,
                        1,
                        format!("include not found: {path}"),
                    )
                    .into());
                }
                let included_stem = file_stem_name(&resolved);
                if included_stem == stem {
                    let inner = fs::read_to_string(&resolved).map_err(|err| {
                        ClppError::at_line(source, 1, 1, format!("read {path}: {err}"))
                    })?;
                    out.push_str(&expand(&inner, &resolved, ctx, seen)?);
                    out.push('\n');
                } else {
                    ctx.requires.push(ModuleRequire {
                        name: included_stem,
                        from_file: file_path.to_string_lossy().into_owned(),
                        to_file: resolved.to_string_lossy().into_owned(),
                    });
                }
                continue;
            }
        }
        if let Some(rest) = trimmed.strip_prefix("#pragma") {
            let rest = rest.trim();
            if rest == "strict" {
                ctx.strict = true;
            } else if rest == "nstrict" {
                ctx.strict = false;
            }
            continue;
        }
        out.push_str(line);
        out.push('\n');
    }
    Ok(out)
}

fn angled(rest: &str) -> Option<String> {
    rest.strip_prefix('<')
        .and_then(|s| s.strip_suffix('>'))
        .map(|s| s.trim().to_string())
}

fn quoted(rest: &str) -> Option<String> {
    rest.strip_prefix('"')
        .and_then(|s| s.strip_suffix('"'))
        .map(|s| s.trim().to_string())
}

pub fn file_stem_name(path: &Path) -> String {
    let mut name = path
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_default();
    let lower = name.to_lowercase();
    for ext in [".clpp", ".clp", ".clh"] {
        if lower.ends_with(ext) {
            name.truncate(name.len() - ext.len());
            break;
        }
    }
    let lower = name.to_lowercase();
    for tag in [
        ".legacy.server",
        ".legacy.client",
        ".legacy",
        ".server",
        ".client",
        ".plugin",
    ] {
        if lower.ends_with(tag) {
            name.truncate(name.len() - tag.len());
            break;
        }
    }
    name
}

pub fn is_script(path: &Path) -> bool {
    script_kind(path).is_some()
}

pub fn script_kind(path: &Path) -> Option<String> {
    let name = path
        .file_name()
        .map(|n| n.to_string_lossy().to_lowercase())
        .unwrap_or_default();
    if name.contains(".server.") {
        Some("server".into())
    } else if name.contains(".client.") {
        Some("client".into())
    } else if name.contains(".plugin.") {
        Some("plugin".into())
    } else {
        None
    }
}

pub fn is_header(path: &Path) -> bool {
    path.extension()
        .and_then(|e| e.to_str())
        .is_some_and(|e| e.eq_ignore_ascii_case("clh"))
}

pub fn to_luau_path(rel: &Path) -> PathBuf {
    let s = rel.to_string_lossy();
    let replaced = if let Some(rest) = s.strip_suffix(".clpp") {
        format!("{rest}.luau")
    } else if let Some(rest) = s.strip_suffix(".clp") {
        format!("{rest}.luau")
    } else if let Some(rest) = s.strip_suffix(".clh") {
        format!("{rest}.luau")
    } else {
        format!("{s}.luau")
    };
    PathBuf::from(replaced)
}

fn canonicalize_or(path: &Path) -> PathBuf {
    path.canonicalize().unwrap_or_else(|_| path.to_path_buf())
}
