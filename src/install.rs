use include_dir::{include_dir, Dir};
use miette::{IntoDiagnostic, Result};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

const EXTENSION_ID: &str = "clpp.clpp-language-0.2.3";
static LANG_PACK: Dir<'_> = include_dir!("$CARGO_MANIFEST_DIR/editors/vscode");

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
            remove_old_packs(&dest_root, EXTENSION_ID);
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
        install_root().join("editors/vscode"),
    ];
    for path in candidates {
        if path.join("package.json").is_file() {
            return Ok(path);
        }
    }
    let dest = install_root().join("editors/vscode");
    extract_language_pack(&dest)?;
    Ok(dest)
}

pub fn install_root() -> PathBuf {
    let home = dirs_home();
    if cfg!(windows) {
        home.join("AppData/Local/Programs/CLPP")
    } else {
        home.join(".local/share/clpp")
    }
}

pub fn setup_machine() -> Result<SetupReport> {
    let root = install_root();
    let bin_dir = if cfg!(windows) {
        root.clone()
    } else {
        dirs_home().join(".local/bin")
    };
    fs::create_dir_all(&root).into_diagnostic()?;
    fs::create_dir_all(&bin_dir).into_diagnostic()?;

    let exe = std::env::current_exe().into_diagnostic()?;
    let dest_exe = bin_dir.join(if cfg!(windows) { "clpp.exe" } else { "clpp" });
    if exe != dest_exe {
        fs::copy(&exe, &dest_exe).into_diagnostic()?;
    }
    if cfg!(windows) {
        let setup = bin_dir.join("clpp-setup.exe");
        if exe != setup {
            fs::copy(&exe, &setup).into_diagnostic()?;
        }
    }

    let pack_dest = root.join("editors/vscode");
    extract_language_pack(&pack_dest)?;

    add_to_user_path(&bin_dir)?;
    let editors = install_language(None).unwrap_or_default();

    Ok(SetupReport {
        compiler: dest_exe,
        pack: pack_dest,
        editors,
        path_dir: bin_dir,
    })
}

#[derive(Debug)]
pub struct SetupReport {
    pub compiler: PathBuf,
    pub pack: PathBuf,
    pub editors: Vec<PathBuf>,
    pub path_dir: PathBuf,
}

fn extract_language_pack(dest: &Path) -> Result<()> {
    if dest.join("package.json").is_file() {
        let _ = fs::remove_dir_all(dest);
    }
    fs::create_dir_all(dest).into_diagnostic()?;
    LANG_PACK.extract(dest).into_diagnostic()?;
    Ok(())
}

fn add_to_user_path(dir: &Path) -> Result<()> {
    let dir_s = dir.display().to_string();
    if cfg!(windows) {
        let script = format!(
            "$dir = '{}'; $user = [Environment]::GetEnvironmentVariable('Path','User'); if (-not $user) {{ $user = '' }}; if ($user -notlike ('*' + $dir + '*')) {{ $joined = if ($user.Trim()) {{ $user.TrimEnd(';') + ';' + $dir }} else {{ $dir }}; [Environment]::SetEnvironmentVariable('Path', $joined, 'User') }}",
            dir_s.replace('\'', "''")
        );
        Command::new("powershell")
            .args(["-NoProfile", "-Command", &script])
            .status()
            .into_diagnostic()?;
    } else {
        let rc = dirs_home().join(".profile");
        let line = format!("export PATH=\"{dir_s}:$PATH\"");
        let existing = fs::read_to_string(&rc).unwrap_or_default();
        if !existing.contains(&dir_s) {
            let mut body = existing;
            if !body.ends_with('\n') && !body.is_empty() {
                body.push('\n');
            }
            body.push_str(&line);
            body.push('\n');
            fs::write(&rc, body).into_diagnostic()?;
        }
    }
    Ok(())
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

fn remove_old_packs(dest_root: &Path, keep: &str) {
    if let Ok(entries) = fs::read_dir(dest_root) {
        for entry in entries.flatten() {
            let name = entry.file_name();
            let name = name.to_string_lossy();
            if name.starts_with("clpp.clpp-language") && name != keep {
                let _ = fs::remove_dir_all(entry.path());
            }
        }
    }
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
