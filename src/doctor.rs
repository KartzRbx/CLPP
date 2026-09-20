//! `clpp doctor` — environment report for editors and Cluaupp.

use crate::config::include_roots;
use serde::Serialize;
use std::env;
use std::fs;
use std::path::PathBuf;
use std::time::UNIX_EPOCH;
use std::process::Command;

#[derive(Serialize)]
pub struct DoctorReport {
    pub version: String,
    pub exe: String,
    pub include_roots: Vec<String>,
    pub luau_lsp: Option<String>,
    pub luau_analyze: Option<String>,
    pub rojo: Option<String>,
    pub cache_dir: String,
}

pub fn report() -> DoctorReport {
    let exe = env::current_exe()
        .map(|p| p.display().to_string())
        .unwrap_or_else(|_| "clpp".into());
    let cwd = env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let roots: Vec<String> = include_roots(&cwd)
        .into_iter()
        .map(|p| p.display().to_string())
        .collect();
    DoctorReport {
        version: env!("CARGO_PKG_VERSION").into(),
        exe,
        include_roots: roots,
        luau_lsp: which("luau-lsp"),
        luau_analyze: which("luau-analyze"),
        rojo: which("rojo"),
        cache_dir: cache_dir().display().to_string(),
    }
}

pub fn cache_dir() -> PathBuf {
    if let Ok(local) = env::var("LOCALAPPDATA") {
        return PathBuf::from(local).join("CLPP").join("cache");
    }
    if let Ok(home) = env::var("HOME") {
        return PathBuf::from(home).join(".cache").join("CLPP");
    }
    PathBuf::from(".clpp-cache")
}

pub fn cached_read(path: &std::path::Path) -> std::io::Result<String> {
    let text = fs::read_to_string(path)?;
    let dir = cache_dir();
    let _ = fs::create_dir_all(&dir);
    let mtime = fs::metadata(path)
        .and_then(|m| m.modified())
        .ok()
        .and_then(|t| t.duration_since(UNIX_EPOCH).ok())
        .map(|d| d.as_secs())
        .unwrap_or(0);
    let key = format!(
        "{:x}-{}",
        fnv1a(path.to_string_lossy().as_bytes()) ^ fnv1a(&mtime.to_le_bytes()),
        path.file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_else(|| "file".into())
            .replace(['\\', '/', ':', '*', '?', '"', '<', '>', '|'], "_")
    );
    let dest = dir.join(format!("{key}.txt"));
    let _ = fs::write(dest, &text);
    Ok(text)
}

fn fnv1a(bytes: &[u8]) -> u64 {
    let mut h: u64 = 0xcbf29ce484222325;
    for b in bytes {
        h ^= u64::from(*b);
        h = h.wrapping_mul(0x100000001b3);
    }
    h
}

fn which(name: &str) -> Option<String> {
    let output = if cfg!(windows) {
        Command::new("where").arg(name).output().ok()?
    } else {
        Command::new("which").arg(name).output().ok()?
    };
    if !output.status.success() {
        return None;
    }
    let text = String::from_utf8_lossy(&output.stdout);
    text.lines().next().map(|s| s.trim().to_string())
}
