use crate::ast::Program;
use crate::codegen::luau::emit;
use crate::parser::parse;
use crate::preprocess::{is_header, preprocess, script_kind, to_luau_path};
use crate::support::{CompileArtifact, CompileRequest};
use miette::{IntoDiagnostic, Result};
use std::fs;
use std::path::{Path, PathBuf};

pub fn compile_file(path: &Path) -> Result<String> {
    Ok(compile_artifact(path)?.luau)
}

pub fn compile_source(source: &str, path: &Path) -> Result<String> {
    Ok(compile_artifact_source(source, path, None)?.luau)
}

pub fn compile_artifact(path: &Path) -> Result<CompileArtifact> {
    let source = fs::read_to_string(path).into_diagnostic()?;
    compile_artifact_source(&source, path, None)
}

pub fn compile_request(request: &CompileRequest) -> Result<CompileArtifact> {
    let path = PathBuf::from(&request.file_name);
    compile_artifact_source(&request.source, &path, request.strict)
}

pub fn compile_artifact_source(
    source: &str,
    path: &Path,
    strict: Option<bool>,
) -> Result<CompileArtifact> {
    let (expanded, mut ctx) = preprocess(source, path)?;
    if is_header(path) {
        ctx.is_header = true;
        ctx.is_script = false;
    }
    if let Some(strict) = strict {
        ctx.strict = strict;
        ctx.nonstrict = !strict;
    }
    let program: Program = parse(&expanded, &path.display().to_string())?;
    crate::semantic::check::check_program(&program, &expanded)?;
    let luau = emit(&program, &ctx);
    let kind = script_kind(path);
    Ok(CompileArtifact {
        ok: true,
        luau,
        file_name: path.display().to_string(),
        output_hint: to_luau_path(Path::new(
            path.file_name()
                .map(|n| n.to_string_lossy())
                .as_deref()
                .unwrap_or("out.clpp"),
        ))
        .display()
        .to_string(),
        script_kind: kind.clone(),
        is_script: ctx.is_script,
        is_header: ctx.is_header,
        rojo_class: rojo_class(ctx.is_header, kind.as_deref()),
        libraries: ctx.libraries,
        error: None,
        diagnostics: Vec::new(),
    })
}

fn rojo_class(is_header: bool, kind: Option<&str>) -> String {
    if is_header {
        return "ModuleScript".into();
    }
    match kind {
        Some("client") => "LocalScript".into(),
        Some("server") | Some("plugin") => "Script".into(),
        _ => "ModuleScript".into(),
    }
}

pub fn build_dir(root: &Path, out_dir: &Path) -> Result<Vec<PathBuf>> {
    let mut written = Vec::new();
    for source in collect_sources(root)? {
        let rel = source.strip_prefix(root).unwrap_or(&source);
        let dest = out_dir.join(to_luau_path(rel));
        if let Some(parent) = dest.parent() {
            fs::create_dir_all(parent).into_diagnostic()?;
        }
        let luau = compile_file(&source)?;
        fs::write(&dest, luau).into_diagnostic()?;
        written.push(dest);
    }
    Ok(written)
}

fn collect_sources(root: &Path) -> Result<Vec<PathBuf>> {
    let mut files = Vec::new();
    visit(root, &mut files)?;
    files.sort();
    Ok(files)
}

fn visit(dir: &Path, files: &mut Vec<PathBuf>) -> Result<()> {
    let skip = ["target", "out", ".git", "node_modules", "tests"];
    let entries = fs::read_dir(dir).into_diagnostic()?;
    for entry in entries {
        let entry = entry.into_diagnostic()?;
        let path = entry.path();
        let name = entry.file_name().to_string_lossy().to_string();
        if path.is_dir() {
            if skip.iter().any(|s| name.eq_ignore_ascii_case(s)) {
                continue;
            }
            visit(&path, files)?;
        } else if is_clpp(&name) {
            files.push(path);
        }
    }
    Ok(())
}

fn is_clpp(name: &str) -> bool {
    let lower = name.to_lowercase();
    lower.ends_with(".clpp") || lower.ends_with(".clp") || lower.ends_with(".clh")
}

pub fn parse_program(source: &str, file_name: &str) -> Result<Program> {
    parse(source, file_name)
}
