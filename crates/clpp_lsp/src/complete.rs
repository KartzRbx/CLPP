//! Completion over a rowan CST. An `Error` node on the cursor line is skipped
//! so a half-typed file still offers members.

use crate::index::SymbolIndex;
use clpp_parser::parse;
use clpp_syntax::{SyntaxKind, SyntaxNode};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CompletionItem {
    pub label: String,
    pub kind: &'static str,
}

/// `offset` is a byte index in `source` (typically after `name.`).
pub fn complete(source: &str, offset: usize, index: &SymbolIndex) -> Vec<CompletionItem> {
    let parsed = parse(source);
    let root = parsed.syntax();
    // Error nodes on this line are incomplete tokens, not a reason to return nothing.
    let _errors_on_line = error_nodes_on_line(&root, source, offset);
    let offset = offset.min(source.len());
    let Some(recv) = receiver_before_dot(&source[..offset]) else {
        return Vec::new();
    };
    let Some(ty) = binding_type(source, recv) else {
        return Vec::new();
    };
    index
        .methods(ty)
        .iter()
        .map(|label| CompletionItem {
            label: label.clone(),
            kind: "method",
        })
        .collect()
}

fn error_nodes_on_line(root: &SyntaxNode, source: &str, offset: usize) -> Vec<SyntaxNode> {
    let line = line_of(source, offset.min(source.len()));
    root.descendants()
        .filter(|n| n.kind() == SyntaxKind::Error && line_of(source, n.text_range().start().into()) == line)
        .collect()
}

fn line_of(source: &str, offset: usize) -> usize {
    source[..offset.min(source.len())].bytes().filter(|b| *b == b'\n').count()
}

fn receiver_before_dot(prefix: &str) -> Option<&str> {
    let trimmed = prefix.trim_end();
    let (head, dot) = trimmed.rsplit_once('.')?;
    if !dot.is_empty() {
        return None;
    }
    head.split(|c: char| !c.is_ascii_alphanumeric() && c != '_')
        .next_back()
        .filter(|s| !s.is_empty())
}

/// `Janitor janitor;` even when that line was wrapped in an `Error` node.
fn binding_type<'a>(source: &'a str, name: &str) -> Option<&'a str> {
    for line in source.lines() {
        let t = line.trim().trim_end_matches(';').trim();
        let mut parts = t.split_whitespace();
        let (Some(ty), Some(var)) = (parts.next(), parts.next()) else {
            continue;
        };
        if parts.next().is_none() && var == name && ty.chars().next().is_some_and(|c| c.is_ascii_alphabetic()) {
            return Some(ty);
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::SymbolIndex;
    use clpp_syntax::SyntaxKind;
    use std::fs;

    #[test]
    fn janitor_dot_completes_outside_the_repo() {
        let dir = std::env::temp_dir().join(format!("clpp-lsp-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        let include = dir.join("include").join("libs");
        fs::create_dir_all(&include).unwrap();
        fs::write(
            include.join("janitor.clh"),
            "struct Janitor {\n    void Add();\n    void Cleanup();\n    void Destroy();\n};\n",
        )
        .unwrap();
        let game = dir.join("game");
        let src_dir = game.join("src");
        fs::create_dir_all(&src_dir).unwrap();
        let file = src_dir.join("Boot.clpp");
        let source = "link @clpp.libs.janitor as Janitor;\nJanitor janitor;\njanitor.\n";
        fs::write(&file, source).unwrap();

        let prev = std::env::var("CLPP_INCLUDE").ok();
        std::env::set_var("CLPP_INCLUDE", dir.join("include"));
        let mut index = SymbolIndex::default();
        index.index_source(source, &file, &game);
        let offset = source.rfind("janitor.").unwrap() + "janitor.".len();
        let parsed = clpp_parser::parse(source);
        let cursor_line_has_error = parsed.syntax().descendants().any(|n| {
            n.kind() == SyntaxKind::Error
                && source[..n.text_range().start().into()].bytes().filter(|b| *b == b'\n').count()
                    == source[..offset].bytes().filter(|b| *b == b'\n').count()
        });
        let items = complete(source, offset, &index);
        match prev {
            Some(v) => std::env::set_var("CLPP_INCLUDE", v),
            None => std::env::remove_var("CLPP_INCLUDE"),
        }
        let _ = fs::remove_dir_all(&dir);

        assert!(cursor_line_has_error, "completion line must be an Error node");
        let labels: Vec<_> = items.into_iter().map(|i| i.label).collect();
        assert_eq!(labels, ["Add", "Cleanup", "Destroy"]);
    }
}
