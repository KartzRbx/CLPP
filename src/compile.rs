use crate::ast::Program;
use crate::codegen::emit::emit_with_map;
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

/// Compile with an explicit optimize switch (baseline D = `false`).
pub fn compile_source_opts(source: &str, path: &Path, optimize: bool) -> Result<String> {
    let art = compile_artifact_source_ex(source, path, None, Some(optimize))?;
    if !art.ok {
        miette::bail!("{}", art.error.unwrap_or_else(|| "compile failed".into()));
    }
    Ok(art.luau)
}

pub fn compile_artifact(path: &Path) -> Result<CompileArtifact> {
    let source = fs::read_to_string(path).into_diagnostic()?;
    compile_artifact_source(&source, path, None)
}

pub fn compile_artifact_with_opts(path: &Path, optimize: bool) -> Result<CompileArtifact> {
    let source = fs::read_to_string(path).into_diagnostic()?;
    compile_artifact_source_ex(&source, path, None, Some(optimize))
}

pub fn compile_request(request: &CompileRequest) -> Result<CompileArtifact> {
    let path = PathBuf::from(&request.file_name);
    compile_with_root(
        &request.source,
        &path,
        request.strict,
        request.optimize,
        request.lib_root.as_deref(),
    )
}

pub fn compile_artifact_source(
    source: &str,
    path: &Path,
    strict: Option<bool>,
) -> Result<CompileArtifact> {
    compile_artifact_source_ex(source, path, strict, None)
}

pub fn compile_artifact_source_ex(
    source: &str,
    path: &Path,
    strict: Option<bool>,
    optimize: Option<bool>,
) -> Result<CompileArtifact> {
    compile_with_root(source, path, strict, optimize, None)
}

fn compile_with_root(
    source: &str,
    path: &Path,
    strict: Option<bool>,
    optimize: Option<bool>,
    lib_root: Option<&str>,
) -> Result<CompileArtifact> {
    if let Some(root) = lib_root {
        if !root.split('.').all(|part| {
            let mut chars = part.chars();
            !matches!(
                part,
                "and"
                    | "break"
                    | "continue"
                    | "do"
                    | "else"
                    | "elseif"
                    | "end"
                    | "false"
                    | "for"
                    | "function"
                    | "if"
                    | "in"
                    | "local"
                    | "nil"
                    | "not"
                    | "or"
                    | "repeat"
                    | "return"
                    | "then"
                    | "true"
                    | "until"
                    | "while"
            ) && matches!(chars.next(), Some(c) if c.is_ascii_alphabetic() || c == '_')
                && chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
        }) {
            return Ok(CompileArtifact::fail_with(
                path.display().to_string(),
                "invalid libRoot",
                vec![crate::diag::diag(
                    crate::diag::CLPP0004,
                    1,
                    1,
                    "libRoot must be a dotted identifier path",
                    "error",
                )],
            ));
        }
    }
    let do_opt = optimize_enabled(optimize);
    let file_name = path.display().to_string();
    let (expanded, mut ctx) = match preprocess(source, path) {
        Ok(pair) => pair,
        Err(err) => {
            return Ok(fail_report(&file_name, err, &[], crate::diag::CLPP0003));
        }
    };
    ctx.lib_root = lib_root.map(str::to_owned);
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
            Err(err) => {
                return Ok(fail_report(
                    &file_name,
                    err,
                    &ctx.line_map,
                    crate::diag::CLPP0001,
                ))
            }
        };
    let mut program = program;
    crate::ast::attach_docs(&mut program, &ctx.comments);
    crate::modules::attach_named_requires(&program, path, &mut ctx);
    let mut diagnostics = parse_diags
        .into_iter()
        .map(|d| remap_diagnostic(d, &ctx.line_map))
        .collect::<Vec<_>>();
    for item in &program.items {
        if let crate::ast::Item::Function(func) = item {
            let has_try = func.body.iter().any(crate::ast::visit::stmt_has_try)
                || func
                    .params
                    .iter()
                    .any(|p| p.default.as_ref().is_some_and(crate::ast::visit::has_try));
            if has_try
                && !func
                    .return_type
                    .as_deref()
                    .is_some_and(|t| t.starts_with("Result<"))
            {
                diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP1103,
                    func.line,
                    func.span.start_col,
                    &func.name,
                    "error",
                ));
            }
        } else if let crate::ast::Item::Decl(decl) = item {
            if decl.value.as_ref().is_some_and(crate::ast::visit::has_try) {
                diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP1103,
                    decl.line,
                    decl.span.start_col,
                    "propagation has no enclosing function",
                    "error",
                ));
            }
        } else if let crate::ast::Item::Destructure { value, .. } = item {
            if crate::ast::visit::has_try(value) {
                diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP1103,
                    1,
                    1,
                    "propagation has no enclosing function",
                    "error",
                ));
            }
        }
    }
    crate::ast::visit::visit_program(&program, &mut |node| {
        if let crate::ast::visit::NodeRef::Stmt(crate::ast::Stmt::Switch { cases, .. }) = node {
            if cases
                .iter()
                .any(|c| c.values.iter().any(crate::ast::visit::has_try))
            {
                let mut diagnostic = crate::diag::diag(
                    crate::diag::CLPP0002,
                    1,
                    1,
                    "Result propagation cannot occur in a switch case label",
                    "error",
                );
                diagnostic.help = Some(
                    "unwrap the Result into a local before switch and use a constant case label"
                        .into(),
                );
                diagnostics.push(diagnostic);
            }
        }
        true
    });
    let mut bound = crate::binder::bind(&program);
    let mut types = crate::session::Session::new().types;
    crate::checker::resolve(&program, &mut bound, &mut types);
    diagnostics.extend(crate::modules::resolve_linked_symbols(
        &program,
        &mut bound.symbols,
        &mut types,
    ));
    diagnostics.extend(
        crate::checker::check_unified(&program, &expanded, &ctx.libraries, &bound, &mut types)
            .into_iter()
            .map(|d| remap_diagnostic(d, &ctx.line_map)),
    );
    let _ = bound;
    diagnostics.retain(|d| {
        !source
            .lines()
            .nth(d.line.saturating_sub(1))
            .is_some_and(|l| l.contains("clpp-ignore"))
    });
    normalize_diagnostics(&mut diagnostics, source);
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
    let opt = if do_opt {
        crate::opt::optimize(&mut program)
    } else {
        crate::opt::OptReport::default()
    };
    let (luau, source_map) = emit_with_map(&program, &ctx);
    let kind = script_kind(path);
    Ok(CompileArtifact {
        contract_version: crate::support::CONTRACT_VERSION.into(),
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
        native_hints: opt.native_hints,
        specialized: opt.specialized,
        layout_hints: opt.layout_hints,
        optimized: do_opt,
    })
}

fn optimize_enabled(explicit: Option<bool>) -> bool {
    if let Some(v) = explicit {
        return v;
    }
    match std::env::var("CLPP_NO_OPT") {
        Ok(v) => !(v == "1" || v.eq_ignore_ascii_case("true")),
        Err(_) => true,
    }
}

/// Normalize legacy checker diagnostics into the public protocol.
pub(crate) fn normalize_diagnostics(diagnostics: &mut [CompileDiagnostic], source: &str) {
    for d in diagnostics {
        d.line = d.line.max(1);
        d.column = d.column.max(1);
        if d.code.is_none() {
            let embedded = d
                .message
                .split('[')
                .nth(1)
                .and_then(|s| s.split(']').next());
            let known = embedded.and_then(crate::diag::explain);
            let code = known.unwrap_or(&crate::diag::CLPP0002);
            d.code = Some(code.id.into());
            d.help.get_or_insert_with(|| code.help.into());
        }
        if d.help.is_none() {
            d.help = d
                .code
                .as_deref()
                .and_then(crate::diag::explain)
                .map(|c| c.help.into());
        }
        let width = source
            .lines()
            .nth(d.line - 1)
            .map(|s| s.chars().count() + 1)
            .unwrap_or(d.column);
        d.span = crate::ast::Span::new(d.line, d.column, d.line, width.max(d.column));
    }
}

fn remap_diagnostic(mut diag: CompileDiagnostic, map: &[usize]) -> CompileDiagnostic {
    diag.line = remap_line(map, diag.line);
    diag
}

fn fail_report(
    file_name: &str,
    err: miette::Report,
    map: &[usize],
    code: crate::diag::Code,
) -> CompileArtifact {
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
            span: {
                let (end_line, end_column) = crate::error::offset_to_line_col(
                    &clpp.src,
                    clpp.span.offset() + clpp.span.len(),
                );
                crate::ast::Span::new(
                    remap_line(map, line),
                    column,
                    remap_line(map, end_line),
                    end_column,
                )
            },
        });
    }
    if diagnostics.is_empty() {
        diagnostics.push(crate::diag::diag(
            crate::diag::CLPP0003,
            1,
            1,
            format!("{err:#}"),
            "error",
        ));
    } else {
        for d in &mut diagnostics {
            d.code = Some(code.id.into());
            d.help = Some(code.help.into());
            if d.span.start_line == 0 {
                d.span = crate::ast::Span::point(d.line, d.column);
            }
        }
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
