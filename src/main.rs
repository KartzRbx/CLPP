use clap::{Parser, Subcommand};
use clpp::compile::{build_dir, compile_artifact, compile_file, compile_request};
use clpp::install::{install_language, setup_machine};
use clpp::support::{language_manifest, CompileArtifact, CompileRequest};
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
        },
        Commands::Install { editor } => {
            let dests = install_language(editor.as_deref())?;
            eprintln!("CL++ editor pack installed:");
            for dest in dests {
                eprintln!("  {}", dest.display());
            }
            eprintln!("Reload the editor window to enable the language.");
        }
        Commands::Setup => run_setup(false)?,
        Commands::Manifest => print_json(&language_manifest())?,
    }
    Ok(())
}

fn run_setup(pause: bool) -> Result<()> {
    let report = setup_machine()?;
    eprintln!("CL++ is installed on this machine.");
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
