//! Luau for `link`. Emit runs only after [clpp_ty::check] reports no AUTH/PAR errors.
//! The shipping emitter is still `clpp::codegen` until cutover.

use clpp_ty::{check, Diagnostic, RunContext};
use std::collections::BTreeSet;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum EmitError {
    /// Semantic safety failed. No Luau is produced.
    Blocked(Vec<Diagnostic>),
}

/// Lower `source` to Luau, or refuse when `clpp_ty` finds a safety violation.
pub fn emit(source: &str, ctx: RunContext) -> Result<String, EmitError> {
    let checked = check(source, ctx);
    if !checked.ok() {
        return Err(EmitError::Blocked(checked.diagnostics));
    }
    Ok(lower(source))
}

pub struct Mapped {
    pub luau: String,
    pub sourcemap: String,
}

/// Luau plus a source map (version 3) whose `names` list is `genLine:srcLine` pairs.
pub fn emit_mapped(source: &str, ctx: RunContext, file: &str) -> Result<Mapped, EmitError> {
    let luau = emit(source, ctx)?;
    let mut pairs = Vec::new();
    for (src_i, raw) in source.lines().enumerate() {
        if raw.trim_start().starts_with("link ") {
            pairs.push(src_i + 1);
        }
    }
    let segments: Vec<String> = pairs.iter().enumerate().map(|(gen, src)| format!("{}:{}", gen + 1, src)).collect();
    let sourcemap = format!(
        "{{\"version\":3,\"file\":\"{file}\",\"sources\":[\"{file}\"],\"mappings\":\"{segs}\"}}",
        segs = segments.join(";")
    );
    Ok(Mapped { luau, sourcemap })
}

fn lower(source: &str) -> String {
    let mut services = BTreeSet::new();
    let mut lines = Vec::new();
    for raw in source.lines() {
        let trimmed = raw.trim().trim_end_matches(';').trim();
        let Some(rest) = trimmed.strip_prefix("link ") else {
            continue;
        };
        let (path, alias) = split_alias(rest);
        if let Some(game) = path.strip_prefix("@game.") {
            let mut parts = game.split('.').filter(|s| !s.is_empty());
            let Some(service) = parts.next() else {
                continue;
            };
            services.insert(service.to_string());
            let tail = parts.collect::<Vec<_>>().join(".");
            let name = alias.unwrap_or(tail.rsplit('.').next().unwrap_or(service));
            let require_path = if tail.is_empty() {
                service.to_string()
            } else {
                format!("{service}.{tail}")
            };
            lines.push(format!("local {name} = require({require_path})"));
        } else if path.starts_with("@clpp.") || path == "@clpp" {
            let bound = alias.unwrap_or(path);
            lines.push(format!("-- clpp prelude: {path} as {bound}"));
        }
    }
    let mut out = String::new();
    for service in services {
        out.push_str(&format!("local {service} = game:GetService(\"{service}\")\n"));
    }
    for line in lines {
        out.push_str(&line);
        out.push('\n');
    }
    out
}

fn split_alias(rest: &str) -> (&str, Option<&str>) {
    if let Some((path, alias)) = rest.split_once(" as ") {
        (path.trim(), Some(alias.trim()))
    } else {
        (rest.trim(), None)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn game_link_is_getservice_and_require() {
        let src = "link @game.ReplicatedStorage.Modules.Combat as CombatModule;\n";
        let luau = emit(src, RunContext::Server).unwrap();
        assert!(luau.contains("local ReplicatedStorage = game:GetService(\"ReplicatedStorage\")"));
        assert!(luau.contains("local CombatModule = require(ReplicatedStorage.Modules.Combat)"));
    }

    #[test]
    fn clpp_link_is_prelude_not_require() {
        let src = "link @clpp.libs.janitor as Janitor;\n";
        let luau = emit(src, RunContext::Module).unwrap();
        assert!(luau.contains("-- clpp prelude: @clpp.libs.janitor as Janitor"));
        assert!(!luau.contains("require("));
        assert!(!luau.contains("GetService"));
    }

    #[test]
    fn auth_violation_blocks_luau() {
        let src = "@server\nvoid Save();\nvoid init() {\n    Save();\n}\nlink @game.ReplicatedStorage.Modules.Combat as CombatModule;\n";
        let err = emit(src, RunContext::Client).unwrap_err();
        match err {
            EmitError::Blocked(diags) => assert!(diags.iter().any(|d| d.code == "CLUAU_AUTH001")),
        }
    }
}
