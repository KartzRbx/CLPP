//! Module interfaces and dependency graphs for the original link syntax.
//! Legacy import helper names are internal; no import keyword is accepted.

use crate::ast::{ImportName, Item, ModuleRequire, Program};
use crate::parser::{parse, parse_for_ide};
use crate::preprocess::{file_stem_name, preprocess, resolve_quoted_include};
use crate::session::CheckedFile;
use crate::symbols::{SymbolId, SymbolKind};
use crate::types::TypeDatabase;
use std::collections::{HashMap, HashSet};
use std::path::Path;

pub fn import_requires(
    file: &mut CheckedFile,
    types: &mut TypeDatabase,
    seen: &mut HashSet<String>,
) {
    let requires = file.ctx.requires.clone();
    for req in requires {
        if !seen.insert(req.to_file.clone()) {
            continue;
        }
        let Some(mut child) = load_module(&req.to_file, types) else {
            continue;
        };
        import_requires(&mut child, types, seen);
        import_named(&mut child, types, seen);
        merge_exports(file, &child, None, types);
    }
}

/// `import { Foo, Bar as B } from "./Foo"` — same path resolver as quoted `#include`.
/// Detect import cycles for diagnostics (RFC 0003 hardening).
pub fn detect_cycles(root: &str, edges: &[(String, String)]) -> Vec<(String, String)> {
    let mut adj: HashMap<String, Vec<String>> = HashMap::new();
    for (a, b) in edges {
        adj.entry(a.clone()).or_default().push(b.clone());
    }
    let mut stack = Vec::new();
    let mut on_stack = HashSet::new();
    let mut seen = HashSet::new();
    let mut cycles = Vec::new();
    fn dfs(
        node: &str,
        adj: &HashMap<String, Vec<String>>,
        stack: &mut Vec<String>,
        on_stack: &mut HashSet<String>,
        seen: &mut HashSet<String>,
        cycles: &mut Vec<(String, String)>,
    ) {
        if !seen.insert(node.to_string()) {
            return;
        }
        stack.push(node.to_string());
        on_stack.insert(node.to_string());
        if let Some(nexts) = adj.get(node) {
            for n in nexts {
                if on_stack.contains(n) {
                    cycles.push((node.to_string(), n.clone()));
                } else {
                    dfs(n, adj, stack, on_stack, seen, cycles);
                }
            }
        }
        on_stack.remove(node);
        stack.pop();
    }
    dfs(
        root,
        &adj,
        &mut stack,
        &mut on_stack,
        &mut seen,
        &mut cycles,
    );
    cycles
}

pub fn import_named(file: &mut CheckedFile, types: &mut TypeDatabase, seen: &mut HashSet<String>) {
    attach_named_requires(&file.program, Path::new(&file.path), &mut file.ctx);
    let imports: Vec<(Vec<ImportName>, String, usize)> = file
        .program
        .items
        .iter()
        .filter_map(|item| match item {
            Item::Import {
                names,
                module,
                line,
                ..
            } => Some((names.clone(), module.clone(), *line)),
            _ => None,
        })
        .collect();
    let from_path = file.path.clone();
    let from = Path::new(&from_path);
    for (names, module, line) in imports {
        if module.starts_with('@') {
            let name = names
                .first()
                .map(|n| n.local_name().to_string())
                .unwrap_or_else(|| "module".into());
            file.ctx.requires.push(crate::ast::ModuleRequire {
                name,
                from_file: from_path.clone(),
                to_file: module,
            });
            continue;
        }
        let Some(resolved) = resolve_quoted_include(&module, from) else {
            file.diagnostics.push(crate::diag::diag(
                crate::diag::CLPP0801,
                line,
                1,
                format!("import not found: {module}"),
                "error",
            ));
            continue;
        };
        let to_file = resolved.to_string_lossy().into_owned();
        let already = !seen.insert(to_file.clone());
        if already {
            if let Some(child) = load_module(&to_file, types) {
                merge_exports(file, &child, Some(&names), types);
            }
            continue;
        }
        let Some(mut child) = load_module(&to_file, types) else {
            continue;
        };
        import_requires(&mut child, types, seen);
        import_named(&mut child, types, seen);
        merge_exports(file, &child, Some(&names), types);
    }
    // Cycle check on requires gathered so far.
    let edges: Vec<(String, String)> = file
        .ctx
        .requires
        .iter()
        .map(|r| (r.from_file.clone(), r.to_file.clone()))
        .collect();
    for (a, b) in detect_cycles(&file.path, &edges) {
        file.diagnostics.push(crate::diag::diag(
            crate::diag::CLPP1001,
            1,
            1,
            format!("import cycle involving {a} → {b}"),
            "error",
        ));
    }
}

pub fn attach_named_requires(program: &Program, from: &Path, ctx: &mut crate::ast::CompileContext) {
    for item in &program.items {
        if let Item::Import { module, .. } = item {
            if let Some(lib) = module.strip_prefix("@clpp.libs.") {
                let lib = crate::names::lib_from_include(&format!("clpp/libs/{lib}.clh"))
                    .unwrap_or_else(|| {
                        let mut c = lib.chars();
                        c.next()
                            .map(|x| x.to_uppercase().to_string() + c.as_str())
                            .unwrap_or_default()
                    });
                if !ctx.libraries.contains(&lib) {
                    ctx.libraries.push(lib);
                }
            }
        }
    }
    for req in named_requires(program, from) {
        if ctx.requires.iter().any(|r| r.to_file == req.to_file) {
            continue;
        }
        ctx.requires.push(req);
    }
}

pub fn named_requires(program: &Program, from: &Path) -> Vec<ModuleRequire> {
    let mut out = Vec::new();
    for item in &program.items {
        let Item::Import { module, names, .. } = item else {
            continue;
        };
        if module.starts_with('@') {
            let name = names
                .first()
                .map(|n| n.local_name().to_string())
                .unwrap_or_else(|| {
                    module
                        .rsplit(['.', '/'])
                        .next()
                        .unwrap_or("module")
                        .to_string()
                });
            out.push(ModuleRequire {
                name,
                from_file: from.to_string_lossy().into_owned(),
                to_file: module.clone(),
            });
            continue;
        }
        let Some(resolved) = resolve_quoted_include(module, from) else {
            continue;
        };
        let stem = file_stem_name(&resolved);
        out.push(ModuleRequire {
            name: names
                .first()
                .map(|n| n.local_name().to_string())
                .unwrap_or(stem),
            from_file: from.to_string_lossy().into_owned(),
            to_file: resolved.to_string_lossy().into_owned(),
        });
    }
    out
}

fn load_module(path: &str, types: &mut TypeDatabase) -> Option<CheckedFile> {
    let path_ref = Path::new(path);
    let source = std::fs::read_to_string(path_ref).ok()?;
    let (expanded, ctx) = preprocess(&source, path_ref).ok()?;
    let (program, _) = match parse(&expanded, path) {
        Ok(p) => (p, Vec::new()),
        Err(_) => parse_for_ide(&expanded, path),
    };
    let mut bound = crate::binder::bind(&program);
    crate::checker::resolve(&program, &mut bound, types);
    Some(CheckedFile {
        path: path.to_string(),
        hash: crate::session::hash_source(&source),
        source,
        program,
        symbols: bound.symbols,
        ctx,
        diagnostics: Vec::new(),
    })
}

fn merge_exports(
    into: &mut CheckedFile,
    from: &CheckedFile,
    only: Option<&[ImportName]>,
    types: &mut TypeDatabase,
) {
    let rename: Option<HashMap<&str, &str>> = only.map(|names| {
        names
            .iter()
            .map(|n| (n.name.as_str(), n.local_name()))
            .collect()
    });
    let wanted: Option<HashSet<&str>> =
        only.map(|names| names.iter().map(|s| s.name.as_str()).collect());
    let export_ids: HashSet<SymbolId> = from
        .symbols
        .symbols
        .iter()
        .filter(|s| wanted.as_ref().is_none_or(|w| w.contains(s.name.as_str())))
        .map(|s| s.id)
        .collect();
    for sym in &from.symbols.symbols {
        if !matches!(
            sym.kind,
            SymbolKind::Struct
                | SymbolKind::Function
                | SymbolKind::TypeAlias
                | SymbolKind::Enum
                | SymbolKind::Field
                | SymbolKind::Method
                | SymbolKind::Constructor
                | SymbolKind::EnumVariant
        ) {
            continue;
        }
        if let Some(wanted) = &wanted {
            let named = wanted.contains(sym.name.as_str());
            let member = sym.owner.is_some_and(|o| export_ids.contains(&o));
            if !named && !member {
                continue;
            }
        }
        let local = rename
            .as_ref()
            .and_then(|m| m.get(sym.name.as_str()).copied())
            .unwrap_or(sym.name.as_str());
        let insert_name = if sym.owner.is_some() {
            sym.name.as_str()
        } else {
            local
        };
        if into
            .symbols
            .lookup(into.symbols.file_scope, insert_name)
            .is_some()
        {
            continue;
        }
        let type_id = if sym.owner.is_none()
            && insert_name != sym.name
            && matches!(
                sym.kind,
                SymbolKind::Struct | SymbolKind::Enum | SymbolKind::TypeAlias
            ) {
            types.define_alias(insert_name, sym.type_id)
        } else {
            sym.type_id
        };
        let id = into.symbols.alloc(
            insert_name.to_string(),
            sym.kind,
            sym.declared_type.clone(),
            sym.span,
            into.symbols.file_scope,
            None,
        );
        if let Some(dst) = into.symbols.get_mut(id) {
            dst.type_id = type_id;
            dst.is_const = sym.is_const;
            dst.is_static = sym.is_static;
            dst.doc = sym.doc.clone();
        }
    }
}

/// Resolve the original link syntax from source interfaces, with a full graph cycle check.
pub fn resolve_linked_symbols(
    program: &Program,
    symbols: &mut crate::symbols::SymbolDatabase,
    types: &mut TypeDatabase,
) -> Vec<crate::support::CompileDiagnostic> {
    fn load(
        program: &Program,
        symbols: &mut crate::symbols::SymbolDatabase,
        types: &mut TypeDatabase,
        stack: &mut Vec<String>,
        cache: &mut HashMap<String, (Program, crate::symbols::SymbolDatabase)>,
        diagnostics: &mut Vec<crate::support::CompileDiagnostic>,
    ) {
        for item in &program.items {
            let Item::Import {
                names,
                module,
                line,
                ..
            } = item
            else {
                continue;
            };
            if module.starts_with('@') {
                continue;
            }
            let Some(resolved) = resolve_quoted_include(module, Path::new(&program.file_name))
            else {
                diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP0801,
                    *line,
                    1,
                    format!("link not found: {module}"),
                    "error",
                ));
                continue;
            };
            let key = resolved
                .canonicalize()
                .unwrap_or_else(|_| resolved.clone())
                .to_string_lossy()
                .into_owned();
            if stack.contains(&key) {
                diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP1001,
                    *line,
                    1,
                    format!("link cycle: {} -> {key}", stack.join(" -> ")),
                    "error",
                ));
                continue;
            }
            let child = if let Some(child) = cache.get(&key) {
                child.clone()
            } else {
                let source = match std::fs::read_to_string(&resolved) {
                    Ok(s) => s,
                    Err(err) => {
                        diagnostics.push(crate::diag::diag(
                            crate::diag::CLPP0801,
                            *line,
                            1,
                            err,
                            "error",
                        ));
                        continue;
                    }
                };
                let (expanded, _) = match preprocess(&source, &resolved) {
                    Ok(p) => p,
                    Err(err) => {
                        diagnostics.push(crate::diag::diag(
                            crate::diag::CLPP0801,
                            *line,
                            1,
                            err,
                            "error",
                        ));
                        continue;
                    }
                };
                let (child, parse_diags) =
                    match crate::parser::parse_with_diagnostics(&expanded, &key) {
                        Ok(p) => p,
                        Err(err) => {
                            diagnostics.push(crate::diag::diag(
                                crate::diag::CLPP0801,
                                *line,
                                1,
                                err,
                                "error",
                            ));
                            continue;
                        }
                    };
                diagnostics.extend(parse_diags);
                let mut bound = crate::binder::bind(&child);
                crate::checker::resolve(&child, &mut bound, types);
                stack.push(key.clone());
                load(&child, &mut bound.symbols, types, stack, cache, diagnostics);
                stack.pop();
                diagnostics.extend(crate::checker::check_unified(
                    &child,
                    &expanded,
                    &[],
                    &bound,
                    types,
                ));
                let entry = (child, bound.symbols);
                cache.insert(key.clone(), entry.clone());
                entry
            };
            let alias = names.first().map(|n| n.local_name()).unwrap_or("Module");
            let exports: Vec<_> = child
                .1
                .symbols
                .iter()
                .filter(|s| {
                    s.owner.is_none()
                        && matches!(
                            s.kind,
                            SymbolKind::Struct
                                | SymbolKind::Enum
                                | SymbolKind::Function
                                | SymbolKind::Variable
                                | SymbolKind::TypeAlias
                        )
                })
                .collect();
            let runtime_exports: Vec<_> = exports
                .iter()
                .filter(|s| s.kind != SymbolKind::TypeAlias)
                .collect();
            let single_class =
                runtime_exports.len() == 1 && runtime_exports[0].kind == SymbolKind::Struct;
            let module_ty = if single_class {
                types.define_alias(alias, runtime_exports[0].type_id)
            } else {
                let ty = types.define_struct(alias, Vec::new());
                for export in exports {
                    let (kind, params, ret) = match types.peel(export.type_id) {
                        crate::types::TypeKind::Function(ft) => (
                            crate::types::MemberKind::Method,
                            ft.params
                                .iter()
                                .enumerate()
                                .map(|(i, t)| (format!("arg{i}"), *t))
                                .collect(),
                            ft.ret,
                        ),
                        _ => (crate::types::MemberKind::Field, Vec::new(), export.type_id),
                    };
                    types.add_member(
                        alias,
                        crate::types::StructMember {
                            name: export.name.clone(),
                            type_id: ret,
                            kind,
                            params,
                            is_static: true,
                            doc: export.doc.clone(),
                        },
                    );
                }
                ty
            };
            let sid = symbols
                .lookup(symbols.file_scope, alias)
                .unwrap_or_else(|| {
                    symbols.alloc(
                        alias.into(),
                        SymbolKind::Module,
                        Some(alias.into()),
                        crate::ast::Span::point(*line, 1),
                        symbols.file_scope,
                        None,
                    )
                });
            if let Some(sym) = symbols.get_mut(sid) {
                sym.type_id = module_ty;
                sym.declared_type = Some(alias.into());
            }
        }
    }
    let key = Path::new(&program.file_name)
        .canonicalize()
        .unwrap_or_else(|_| Path::new(&program.file_name).to_path_buf())
        .to_string_lossy()
        .into_owned();
    let mut diagnostics = Vec::new();
    load(
        program,
        symbols,
        types,
        &mut vec![key],
        &mut HashMap::new(),
        &mut diagnostics,
    );
    diagnostics
}
