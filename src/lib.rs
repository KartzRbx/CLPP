pub mod analysis;
pub mod ast;
pub mod binder;
pub mod builtins;
pub mod checker;
pub mod codegen;
pub mod compile;
pub mod config;
pub mod diag;
pub mod diagnostics;
pub mod doctor;
pub mod driver;
pub mod emitter;
pub mod error;
pub mod fmt;
pub mod install;
pub mod intent;
pub mod lint;
pub mod lsp;
pub mod modules;
pub mod names;
pub mod opt;
pub mod parser;
pub mod platform;
pub mod preprocess;
pub mod roblox;
pub mod session;
pub mod support;
pub mod symbols;
pub mod types;
pub mod watch;

pub use compile::{
    build_dir, compile_artifact, compile_artifact_source, compile_artifact_source_ex,
    compile_artifact_with_opts, compile_file, compile_request, compile_source, compile_source_opts,
};
pub use support::{CompileArtifact, CompileRequest, LanguageManifest};
