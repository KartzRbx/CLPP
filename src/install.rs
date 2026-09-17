use miette::{IntoDiagnostic, Result};
use std::fs;
use std::path::{Path, PathBuf};

const EXTENSION_ID: &str = "clpp.clpp-language-0.1.0";

struct Editor {
    id: &'static str,
    aliases: &'static [&'static str],
    label: &'static str,
    /// Paths relative to the user home directory.
    dirs: &'static [&'static str],
}

const EDITORS: &[Editor] = &[
    Editor {
        id: "cursor",
        aliases: &["cursor"],
        label: "Cursor",
        dirs: &[".cursor/extensions"],
    },
    Editor {
        id: "code",
        aliases: &["code", "vscode", "vs"],
        label: "VS Code",
        dirs: &[".vscode/extensions"],
    },
    Editor {
        id: "insiders",
        aliases: &["insiders", "code-insiders", "vscode-insiders"],
        label: "VS Code Insiders",
        dirs: &[".vscode-insiders/extensions"],
    },
    Editor {
        id: "codium",
        aliases: &["codium", "vscodium"],
        label: "VSCodium",
        dirs: &[".vscode-oss/extensions"],
    },
    Editor {
        id: "windsurf",
        aliases: &["windsurf", "codeium"],
        label: "Windsurf",
        dirs: &[".windsurf/extensions"],
    },
];

pub fn install_language(editor: Option<&str>) -> Result<Vec<PathBuf>> {
    let source = language_pack_dir()?;
    let home = dirs_home();
    let selected = selected_editors(editor);
    if selected.is_empty() {
        miette::bail!(
            "editor desconhecido. Use: cursor, code, insiders, codium, windsurf (ou omita para todos)"
        );
    }
    let force = editor.is_some() && editor != Some("all");
    let mut written = Vec::new();
    for spec in selected {
        for rel in spec.dirs {
            let dest_root = home.join(rel);
            if !force && spec.id != "code" && spec.id != "cursor" {
                let product = dest_root.parent();
                if !product.is_some_and(|p| p.is_dir()) {
                    continue;
                }
            }
            fs::create_dir_all(&dest_root).into_diagnostic()?;
            let dest = dest_root.join(EXTENSION_ID);
            copy_dir(&source, &dest)?;
            written.push(dest);
        }
    }
    if written.is_empty() {
        miette::bail!("nenhum editor-alvo para instalar");
    }
    Ok(written)
}

pub fn supported_editors() -> Vec<&'static str> {
    EDITORS.iter().map(|e| e.label).collect()
}

pub fn language_pack_dir() -> Result<PathBuf> {
    let candidates = [
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("editors/vscode"),
        current_exe_dir()?.join("editors/vscode"),
        current_exe_dir()?.join("../editors/vscode"),
    ];
    for path in candidates {
        if path.join("package.json").is_file() {
            return Ok(path);
        }
    }
    miette::bail!(
        "language pack not found (editors/vscode). Build from the CL++ repository."
    )
}

fn selected_editors(editor: Option<&str>) -> Vec<&'static Editor> {
    let want = editor.map(|s| s.trim().to_ascii_lowercase());
    match want.as_deref() {
        None | Some("") | Some("all") => EDITORS.iter().collect(),
        Some(want) => EDITORS
            .iter()
            .filter(|e| e.id == want || e.aliases.iter().any(|a| *a == want))
            .collect(),
    }
}

fn current_exe_dir() -> Result<PathBuf> {
    let exe = std::env::current_exe().into_diagnostic()?;
    Ok(exe.parent().unwrap_or(Path::new(".")).to_path_buf())
}

fn dirs_home() -> PathBuf {
    std::env::var_os("USERPROFILE")
        .or_else(|| std::env::var_os("HOME"))
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("."))
}

fn copy_dir(from: &Path, to: &Path) -> Result<()> {
    fs::create_dir_all(to).into_diagnostic()?;
    for entry in fs::read_dir(from).into_diagnostic()? {
        let entry = entry.into_diagnostic()?;
        let src = entry.path();
        let dest = to.join(entry.file_name());
        if src.is_dir() {
            copy_dir(&src, &dest)?;
        } else {
            fs::copy(&src, &dest).into_diagnostic()?;
        }
    }
    Ok(())
}
