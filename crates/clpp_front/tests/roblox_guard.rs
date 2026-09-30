//! Fails if a host-runtime name lands in the phase-1 front end.
//!
//! The compiler core stays free of one platform the way a language's
//! prelude stays free of a DOM library. A later target package owns those names.

use std::fs;
use std::path::Path;

const NEEDLES: &[&str] = &[
    "roblox",
    "rojo",
    "janitor",
    "getservice",
    "rbxscriptsignal",
    "vector3",
    "cframe",
    "replicatedstorage",
    "localscript",
    "modulescript",
    "datamodel",
    "instance",
    "workspace",
];

#[test]
fn front_end_source_has_no_host_runtime_names() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("src");
    let mut hits = Vec::new();
    walk(&root, &mut hits);
    assert!(
        hits.is_empty(),
        "host-runtime names in the front end:\n{}",
        hits.join("\n")
    );
}

fn walk(dir: &Path, hits: &mut Vec<String>) {
    for entry in fs::read_dir(dir).unwrap() {
        let entry = entry.unwrap();
        let path = entry.path();
        if path.is_dir() {
            walk(&path, hits);
            continue;
        }
        let text = fs::read_to_string(&path).unwrap_or_default();
        let lower = text.to_ascii_lowercase();
        for needle in NEEDLES {
            if let Some(at) = lower.find(needle) {
                hits.push(format!("{}: {needle} at byte {at}", path.display()));
            }
        }
        if word_boundary(&lower, "game") {
            hits.push(format!("{}: game", path.display()));
        }
        if lower.contains("task.") {
            hits.push(format!("{}: task.", path.display()));
        }
    }
}

fn word_boundary(text: &str, word: &str) -> bool {
    text.match_indices(word).any(|(i, _)| {
        let before = text[..i].chars().next_back().unwrap_or(' ');
        let after = text[i + word.len()..].chars().next().unwrap_or(' ');
        !before.is_ascii_alphanumeric() && before != '_' && !after.is_ascii_alphanumeric() && after != '_'
    })
}
