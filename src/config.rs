//! `.clpprc` project file (include roots, receiver mode).

use serde::Deserialize;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Default, Deserialize)]
pub struct ProjectConfig {
    #[serde(default)]
    pub include: Vec<String>,
    #[serde(default)]
    pub strict_receiver: bool,
}

pub fn load_clpprc(start: &Path) -> ProjectConfig {
    let mut dir = if start.is_file() {
        start.parent().unwrap_or(start).to_path_buf()
    } else {
        start.to_path_buf()
    };
    for _ in 0..16 {
        let candidate = dir.join(".clpprc");
        if candidate.is_file() {
            if let Ok(text) = fs::read_to_string(&candidate) {
                if let Ok(cfg) = serde_json::from_str::<ProjectConfig>(&text) {
                    return cfg;
                }
            }
        }
        if !dir.pop() {
            break;
        }
    }
    ProjectConfig::default()
}

pub fn include_roots(start: &Path) -> Vec<PathBuf> {
    let cfg = load_clpprc(start);
    let mut roots = Vec::new();
    let base = if start.is_file() {
        start.parent().unwrap_or(start).to_path_buf()
    } else {
        start.to_path_buf()
    };
    for inc in cfg.include {
        let p = PathBuf::from(&inc);
        roots.push(if p.is_absolute() { p } else { base.join(p) });
    }
    roots
}
