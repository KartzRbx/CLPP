use crate::ast::Program;
use crate::codegen::luau::emit;
use crate::error::ClppError;
use crate::parser::{parse, parse_with_diagnostics};
use crate::preprocess::{is_header, preprocess, remap_line, script_kind, to_luau_path};
use crate::support::{CompileArtifact, CompileDiagnostic, CompileRequest};
use miette::{IntoDiagnostic, Result};
use std::fs;
use std::path::{Path, PathBuf};

pub fn compile_file(path: &Path) -> Result<String> {
    let art = compile_artifact(path)?;
    if !art.ok {
        miette::bail!("{}", art.error.unwrap_or_else(|| "compile failed".into()));
    }
    Ok(art.luau)
}

pub fn compile_source(source: &str, path: &Path) -> Result<String> {
    let art = compile_artifact_source(source, path, None)?;
    if !art.ok {
        miette::bail!("{}", art.error.unwrap_or_else(|| "compile failed".into()));
    }
    Ok(art.luau)
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
    let file_name = path.display().to_string();
    let (expanded, mut ctx) = match preprocess(source, path) {
        Ok(pair) => pair,
        Err(err) => {
            return Ok(fail_report(&file_name, err, &[]));
        }
    };
    if is_header(path) {
        ctx.is_header = true;
        ctx.is_script = false;
    }
    if let Some(strict) = strict {
        ctx.strict = strict;
        ctx.nonstrict = !strict;
    }
    let (program, parse_diags): (Program, Vec<CompileDiagnostic>) =
        match parse_with_diagnostics(&expanded, &file_name) {
            Ok(pair) => pair,
            Err(err) => return Ok(fail_report(&file_name, err, &ctx.line_map)),
        };
    let mut diagnostics = parse_diags
        .into_iter()
        .map(|d| remap_diagnostic(d, &ctx.line_map))
        .collect::<Vec<_>>();
    match crate::semantic::check::check_program_ex(&program, &expanded, &ctx.libraries) {
        Ok(items) => diagnostics.extend(items.into_iter().map(|d| remap_diagnostic(d, &ctx.line_map))),
        Err(err) => return Ok(fail_report(&file_name, err, &ctx.line_map)),
    };
    diagnostics.retain(|d| {
        !source
            .lines()
            .nth(d.line.saturating_sub(1))
            .is_some_and(|l| l.contains("clpp-ignore"))
    });
    let errors: Vec<_> = diagnostics
        .iter()
        .filter(|d| d.severity != "warning")
        .cloned()
        .collect();
    if !errors.is_empty() {
        let message = errors
            .first()
            .map(|d| d.message.clone())
            .unwrap_or_else(|| "compile failed".into());
        return Ok(CompileArtifact::fail_with(file_name, message, diagnostics));
    }
    let luau = emit(&program, &ctx);
    let source_map = build_source_map(&luau, &file_name);
    let kind = script_kind(path);
    Ok(CompileArtifact {
        ok: true,
        luau,
        file_name,
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
        diagnostics,
        source_map,
    })
}

fn build_source_map(luau: &str, file_name: &str) -> Vec<crate::support::SourceMapLine> {
    let file = Path::new(file_name)
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_else(|| file_name.to_string());
    let mut current = 1usize;
    let mut out = Vec::new();
    for (i, line) in luau.lines().enumerate() {
        let luau_line = i + 1;
        let trimmed = line.trim_start();
        if let Some(rest) = trimmed.strip_prefix("-- ") {
            if let Some((name, num)) = rest.rsplit_once(':') {
                if Path::new(name).file_name().map(|n| n.to_string_lossy()) == Path::new(&file).file_name().map(|n| n.to_string_lossy())
                    || name == file
                    || name.ends_with(&file)
                {
                    if let Ok(n) = num.parse::<usize>() {
                        current = n;
                    }
                }
            }
        }
        out.push(crate::support::SourceMapLine {
            luau_line,
            clpp_line: current,
            file: file.clone(),
        });
    }
    out
}

fn remap_diagnostic(mut diag: CompileDiagnostic, map: &[usize]) -> CompileDiagnostic {
    diag.line = remap_line(map, diag.line);
    diag
}

fn fail_report(file_name: &str, err: miette::Report, map: &[usize]) -> CompileArtifact {
    let mut diagnostics = Vec::new();
    if let Some(clpp) = err.downcast_ref::<ClppError>() {
        let (line, column) = clpp.line_col();
        diagnostics.push(CompileDiagnostic {
            message: clpp.message.clone(),
            line: remap_line(map, line),
            column,
            severity: "error".into(),
            code: None,
            help: None,
        });
    }
    CompileArtifact::fail_with(file_name, format!("{err:#}"), diagnostics)
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
