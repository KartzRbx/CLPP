use clap::{Parser, Subcommand};
use clpp::analysis::{
    code_actions_request, complete_request, definition_request, folding_request, format_request,
    highlight_request, hover_request, inlay_request, references_request, signature_request,
    symbols_request, workspace_symbols, PositionRequest,
};
use clpp::compile::{build_dir, compile_artifact, compile_file, compile_request};
use clpp::fmt::format_source;
use clpp::install::{install_language, setup_machine};
use clpp::support::{language_manifest, CompileArtifact, CompileRequest};
use clpp::watch::watch_dir;
use miette::{IntoDiagnostic, Result};
use std::fs;
use std::io::{self, Read, Write};
use std::path::PathBuf;

#[derive(Parser)]
#[command(
    name = "clpp",
    version,
    about = "CL++ compiler — C++-inspired language for Luau. Stable API for Cluaupp."
)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Compile a single .clh / .clp / .clpp file
    Compile {
        file: PathBuf,
        #[arg(short, long)]
        output: Option<PathBuf>,
        /// Emit CompileArtifact JSON (Cluaupp contract)
        #[arg(long)]
        json: bool,
    },
    /// Compile every CL++ source under a directory
    Build {
        #[arg(default_value = ".")]
        root: PathBuf,
        #[arg(short, long, default_value = "out")]
        output: PathBuf,
        #[arg(long)]
        json: bool,
    },
    /// Stable Cluaupp API (JSON in/out)
    Api {
        #[command(subcommand)]
        action: ApiCommand,
    },
    /// Install highlighting, icon, and IntelliSense (VS Code, Cursor, Insiders, VSCodium, Windsurf)
    Install {
        /// cursor | code | insiders | codium | windsurf  (all when omitted)
        #[arg(long)]
        editor: Option<String>,
    },
    /// Install the CL++ compiler and language pack on this machine
    Setup,
    /// Language manifest (extensions, tags, operators)
    Manifest,
    /// Format a .clpp / .clp / .clh file (indent)
    Fmt {
        file: PathBuf,
        #[arg(long)]
        write: bool,
    },
    /// Recompile a directory when sources change
    Watch {
        #[arg(default_value = ".")]
        root: PathBuf,
        #[arg(short, long, default_value = "out")]
        output: PathBuf,
    },
}

#[derive(Subcommand)]
enum ApiCommand {
    /// Compile source. JSON on stdin: { "source", "fileName", "strict?" }
    Compile {
        #[arg(short, long)]
        file: Option<PathBuf>,
    },
    /// Language metadata
    Manifest,
    /// Completions at a 1-based line/column. JSON stdin: PositionRequest
    Complete,
    /// Hover at a position
    Hover,
    /// Document symbols / outline
    Symbols,
    /// Go to definition
    Definition,
    /// Find references
    References,
    /// Signature help
    Signature,
    /// Format source (JSON { source } or raw)
    Format,
    /// Inlay hints
    Inlay,
    /// Folding ranges
    Folding,
    /// Document highlight
    Highlight,
    /// Workspace symbols
    WorkspaceSymbols,
    /// Code actions / quickfix
    Actions,
}

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    let stem = std::env::current_exe()
        .ok()
        .and_then(|p| {
            p.file_stem()
                .map(|s| s.to_string_lossy().to_ascii_lowercase())
        })
        .unwrap_or_default();
    if stem == "clpp-setup" && args.len() <= 1 {
        return run_setup(true);
    }
    let cli = Cli::parse();
    match cli.command {
        Commands::Compile {
            file,
            output,
            json,
        } => {
            if json {
                match compile_artifact(&file) {
                    Ok(art) => {
                        print_json(&art)?;
                        if !art.ok {
                            std::process::exit(1);
                        }
                    }
                    Err(err) => {
                        print_json(&CompileArtifact::fail(file.display().to_string(), format!("{err:#}")))?;
                        std::process::exit(1);
                    }
                }
            } else {
                let luau = compile_file(&file)?;
                if let Some(path) = output {
                    if let Some(parent) = path.parent() {
                        fs::create_dir_all(parent).into_diagnostic()?;
                    }
                    fs::write(path, luau).into_diagnostic()?;
                } else {
                    io::stdout().write_all(luau.as_bytes()).into_diagnostic()?;
                }
            }
        }
        Commands::Build {
            root,
            output,
            json,
        } => {
            let written = build_dir(&root, &output)?;
            if json {
                print_json(&serde_json::json!({
                    "ok": true,
                    "count": written.len(),
                    "output": output,
                    "files": written,
                }))?;
            } else {
                eprintln!("compiled {} file(s) → {}", written.len(), output.display());
                for path in written {
                    eprintln!("  {}", path.display());
                }
            }
        }
        Commands::Api { action } => match action {
            ApiCommand::Compile { file } => {
                let request = if let Some(path) = file {
                    let source = fs::read_to_string(&path).into_diagnostic()?;
                    CompileRequest {
                        source,
                        file_name: path.display().to_string(),
                        strict: None,
                    }
                } else {
                    let mut buf = String::new();
                    io::stdin().read_to_string(&mut buf).into_diagnostic()?;
                    serde_json::from_str(&buf).into_diagnostic()?
                };
                match compile_request(&request) {
                    Ok(art) => {
                        print_json(&art)?;
                        if !art.ok {
                            std::process::exit(1);
                        }
                    }
                    Err(err) => {
                        print_json(&CompileArtifact::fail(&request.file_name, format!("{err:#}")))?;
                        std::process::exit(1);
                    }
                }
            }
            ApiCommand::Manifest => print_json(&language_manifest())?,
            ApiCommand::Complete => {
                let req = read_position()?;
                print_json(&complete_request(&req))?;
            }
            ApiCommand::Hover => {
                let req = read_position()?;
                print_json(&hover_request(&req))?;
            }
            ApiCommand::Symbols => {
                let req = read_position()?;
                print_json(&symbols_request(&req))?;
            }
            ApiCommand::Definition => {
                let req = read_position()?;
                print_json(&definition_request(&req))?;
            }
            ApiCommand::References => {
                let req = read_position()?;
                print_json(&references_request(&req))?;
            }
            ApiCommand::Signature => {
                let req = read_position()?;
                print_json(&signature_request(&req))?;
            }
            ApiCommand::Format => {
                let req = read_position_or_source()?;
                print_json(&format_request(&req.source))?;
            }
            ApiCommand::Inlay => {
                let req = read_position()?;
                print_json(&inlay_request(&req))?;
            }
            ApiCommand::Folding => {
                let req = read_position()?;
                print_json(&folding_request(&req))?;
            }
            ApiCommand::Highlight => {
                let req = read_position()?;
                print_json(&highlight_request(&req))?;
            }
            ApiCommand::WorkspaceSymbols => {
                let req = read_position()?;
                print_json(&workspace_symbols(&req))?;
            }
            ApiCommand::Actions => {
                let req = read_position()?;
                print_json(&code_actions_request(&req))?;
            }
        },
        Commands::Install { editor } => {
            let dests = install_language(editor.as_deref())?;
            eprintln!("CL++ {} editor pack installed:", env!("CARGO_PKG_VERSION"));
            for dest in dests {
                eprintln!("  {}", dest.display());
            }
            eprintln!("Reload the editor window to enable the language.");
        }
        Commands::Setup => run_setup(false)?,
        Commands::Manifest => print_json(&language_manifest())?,
        Commands::Fmt { file, write } => {
            let source = fs::read_to_string(&file).into_diagnostic()?;
            let formatted = format_source(&source);
            if write {
                fs::write(&file, formatted).into_diagnostic()?;
            } else {
                io::stdout().write_all(formatted.as_bytes()).into_diagnostic()?;
            }
        }
        Commands::Watch { root, output } => watch_dir(&root, &output)?,
    }
    Ok(())
}

fn read_position() -> Result<PositionRequest> {
    let mut buf = String::new();
    io::stdin().read_to_string(&mut buf).into_diagnostic()?;
    serde_json::from_str(&buf).into_diagnostic()
}

fn read_position_or_source() -> Result<PositionRequest> {
    let mut buf = String::new();
    io::stdin().read_to_string(&mut buf).into_diagnostic()?;
    if let Ok(req) = serde_json::from_str::<PositionRequest>(&buf) {
        return Ok(req);
    }
    Ok(PositionRequest {
        source: buf,
        file_name: "input.clpp".into(),
        line: 1,
        column: 1,
    })
}

fn run_setup(pause: bool) -> Result<()> {
    let report = setup_machine()?;
    eprintln!("CL++ {} is installed on this machine.", report.version);
    eprintln!("  compiler: {}", report.compiler.display());
    eprintln!("  PATH:     {}", report.path_dir.display());
    eprintln!("  pack:     {}", report.pack.display());
    for dest in &report.editors {
        eprintln!("  editor:   {}", dest.display());
    }
    eprintln!("Open a new terminal, then run: clpp --help");
    eprintln!("Reload Cursor / VS Code to enable highlighting and IntelliSense.");
    if pause {
        eprintln!();
        eprint!("Press Enter to close...");
        let _ = io::stdout().flush();
        let mut buf = String::new();
        let _ = io::stdin().read_line(&mut buf);
    }
    Ok(())
}

fn print_json(value: &impl serde::Serialize) -> Result<()> {
    let body = serde_json::to_string_pretty(value).into_diagnostic()?;
    println!("{body}");
    Ok(())
}
