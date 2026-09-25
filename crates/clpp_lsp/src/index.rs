//! Reads files named by `link` and records struct methods under the alias.

use clpp_cli::path_resolver;
use clpp_parser::parse;
use clpp_syntax::SyntaxKind;
use std::collections::HashMap;
use std::fs;
use std::path::Path;

#[derive(Debug, Default, Clone)]
pub struct SymbolIndex {
    /// Type or alias → method names.
    methods: HashMap<String, Vec<String>>,
}

impl SymbolIndex {
    pub fn methods(&self, type_name: &str) -> &[String] {
        self.methods.get(type_name).map(Vec::as_slice).unwrap_or(&[])
    }

    /// Index `source` and every `link` it names. `from_file` / `project_root` feed the resolver.
    pub fn index_source(&mut self, source: &str, from_file: &Path, project_root: &Path) {
        self.absorb_structs(source, None);
        let parsed = parse(source);
        let root = parsed.syntax();
        for link in root.children().filter(|n| n.kind() == SyntaxKind::LinkStmt) {
            let text = link.to_string();
            let Some((spec, alias)) = split_link(&text) else {
                continue;
            };
            let Ok(path) = path_resolver::resolve_link(spec, from_file, project_root) else {
                continue;
            };
            let Ok(body) = fs::read_to_string(&path) else {
                continue;
            };
            self.absorb_structs(&body, alias);
        }
    }

    fn absorb_structs(&mut self, source: &str, alias: Option<&str>) {
        for (name, methods) in extract_struct_methods(source) {
            if let Some(alias) = alias {
                self.methods.insert(alias.to_string(), methods.clone());
            }
            self.methods.insert(name, methods);
        }
    }
}

fn split_link(text: &str) -> Option<(&str, Option<&str>)> {
    let rest = text.trim().strip_prefix("link")?.trim();
    let (path, alias) = if let Some((path, alias)) = rest.split_once(" as ") {
        (path.trim(), Some(alias.trim().trim_end_matches(';').trim()))
    } else {
        (rest.trim().trim_end_matches(';').trim(), None)
    };
    if path.is_empty() {
        None
    } else {
        Some((path, alias.filter(|a| !a.is_empty())))
    }
}

/// `struct Name { void Add(); }` → methods. Fields with a space are skipped.
pub fn extract_struct_methods(source: &str) -> Vec<(String, Vec<String>)> {
    let mut out = Vec::new();
    let mut current: Option<(String, Vec<String>)> = None;
    for line in source.lines() {
        let t = line.trim().trim_end_matches(';').trim();
        if let Some(rest) = t.strip_prefix("struct ") {
            if let Some(done) = current.take() {
                out.push(done);
            }
            let name = rest.split([' ', '{']).next().unwrap_or("").trim();
            if !name.is_empty() {
                current = Some((name.to_string(), Vec::new()));
            }
            continue;
        }
        if t.starts_with('}') {
            if let Some(done) = current.take() {
                out.push(done);
            }
            continue;
        }
        let Some((_, methods)) = current.as_mut() else {
            continue;
        };
        let rest = t.strip_prefix("void ").unwrap_or(t);
        let name = rest.split('(').next().unwrap_or("").trim();
        if !name.is_empty() && name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_') {
            methods.push(name.to_string());
        }
    }
    if let Some(done) = current {
        out.push(done);
    }
    out
}
