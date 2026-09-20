use crate::compile::compile_file;
use miette::{IntoDiagnostic, Result};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::thread;
use std::time::Duration;

pub fn watch_dir(root: &Path, out_dir: &Path) -> Result<()> {
    eprintln!(
        "watching {} → {} (Ctrl+C to stop)",
        root.display(),
        out_dir.display()
    );
    let mut mtimes: HashMap<PathBuf, u64> = HashMap::new();
    loop {
        let mut dirty = Vec::new();
        collect(root, &mut dirty)?;
        for path in dirty {
            let stamp = file_stamp(&path).unwrap_or(0);
            if mtimes.get(&path).copied() != Some(stamp) {
                mtimes.insert(path.clone(), stamp);
                match compile_file(&path) {
                    Ok(luau) => {
                        let rel = path.strip_prefix(root).unwrap_or(&path);
                        let dest = out_dir.join(rel).with_extension("luau");
                        if let Some(parent) = dest.parent() {
                            fs::create_dir_all(parent).into_diagnostic()?;
                        }
                        fs::write(&dest, luau).into_diagnostic()?;
                        eprintln!("compiled {}", path.display());
                    }
                    Err(err) => eprintln!("error {}: {err}", path.display()),
                }
            }
        }
        thread::sleep(Duration::from_millis(500));
    }
}

fn collect(dir: &Path, files: &mut Vec<PathBuf>) -> Result<()> {
    let skip = ["target", "out", ".git", "node_modules", "tests"];
    for entry in fs::read_dir(dir).into_diagnostic()? {
        let entry = entry.into_diagnostic()?;
        let path = entry.path();
        let name = entry.file_name().to_string_lossy().to_string();
        if path.is_dir() {
            if skip.iter().any(|s| name.eq_ignore_ascii_case(s)) {
                continue;
            }
            collect(&path, files)?;
        } else {
            let lower = name.to_lowercase();
            if lower.ends_with(".clpp") || lower.ends_with(".clp") || lower.ends_with(".clh") {
                files.push(path);
            }
        }
    }
    Ok(())
}

fn file_stamp(path: &Path) -> Option<u64> {
    let meta = fs::metadata(path).ok()?;
    meta.modified()
        .ok()?
        .duration_since(std::time::UNIX_EPOCH)
        .ok()
        .map(|d| d.as_secs())
}
