use serde::{Deserialize, Serialize};

pub const LANGUAGE_ID: &str = "clpp";
pub const LANGUAGE_NAME: &str = "CL++";
pub const EXTENSIONS: &[&str] = &[
    ".clpp", ".clp", ".clh", ".flare", ".mint", ".bloom", ".helm", ".shift", ".hive", ".axiom",
];

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompileDiagnostic {
    pub message: String,
    pub line: usize,
    pub column: usize,
    pub severity: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub code: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub help: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompileRequest {
    pub source: String,
    #[serde(default = "default_file_name", alias = "fileName")]
    pub file_name: String,
    #[serde(default)]
    pub strict: Option<bool>,
}

fn default_file_name() -> String {
    "input.clpp".into()
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompileArtifact {
    pub ok: bool,
    pub luau: String,
    #[serde(rename = "fileName")]
    pub file_name: String,
    #[serde(rename = "outputHint")]
    pub output_hint: String,
    #[serde(rename = "scriptKind")]
    pub script_kind: Option<String>,
    #[serde(rename = "isScript")]
    pub is_script: bool,
    #[serde(rename = "isHeader")]
    pub is_header: bool,
    #[serde(rename = "rojoClass")]
    pub rojo_class: String,
    pub libraries: Vec<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub diagnostics: Vec<CompileDiagnostic>,
    #[serde(default, skip_serializing_if = "Vec::is_empty", rename = "sourceMap")]
    pub source_map: Vec<SourceMapLine>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SourceMapLine {
    #[serde(rename = "luauLine")]
    pub luau_line: usize,
    #[serde(rename = "clppLine")]
    pub clpp_line: usize,
    pub file: String,
}

impl CompileArtifact {
    pub fn fail(file_name: impl Into<String>, error: impl Into<String>) -> Self {
        Self {
            ok: false,
            luau: String::new(),
            file_name: file_name.into(),
            output_hint: String::new(),
            script_kind: None,
            is_script: false,
            is_header: false,
            rojo_class: "ModuleScript".into(),
            libraries: Vec::new(),
            error: Some(error.into()),
            diagnostics: Vec::new(),
            source_map: Vec::new(),
        }
    }

    pub fn fail_with(
        file_name: impl Into<String>,
        error: impl Into<String>,
        diagnostics: Vec<CompileDiagnostic>,
    ) -> Self {
        Self {
            ok: false,
            luau: String::new(),
            file_name: file_name.into(),
            output_hint: String::new(),
            script_kind: None,
            is_script: false,
            is_header: false,
            rojo_class: "ModuleScript".into(),
            libraries: Vec::new(),
            error: Some(error.into()),
            diagnostics,
            source_map: Vec::new(),
        }
    }
}

impl Default for CompileDiagnostic {
    fn default() -> Self {
        Self {
            message: String::new(),
            line: 1,
            column: 1,
            severity: "error".into(),
            code: None,
            help: None,
        }
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct LanguageManifest {
    pub name: &'static str,
    pub id: &'static str,
    pub version: &'static str,
    pub extensions: &'static [&'static str],
    pub tags: &'static [FileTag],
    pub builtins: Vec<&'static str>,
    pub operators: &'static [OperatorMap],
    pub io: &'static [IoMap],
}

#[derive(Debug, Clone, Serialize)]
pub struct FileTag {
    pub pattern: &'static str,
    #[serde(rename = "scriptKind")]
    pub script_kind: &'static str,
    #[serde(rename = "rojoClass")]
    pub rojo_class: &'static str,
}

#[derive(Debug, Clone, Serialize)]
pub struct OperatorMap {
    pub clpp: &'static str,
    pub luau: &'static str,
    pub meaning: &'static str,
}

#[derive(Debug, Clone, Serialize)]
pub struct IoMap {
    pub clpp: &'static str,
    pub luau: &'static str,
}

pub fn language_manifest() -> LanguageManifest {
    LanguageManifest {
        name: LANGUAGE_NAME,
        id: LANGUAGE_ID,
        version: env!("CARGO_PKG_VERSION"),
        extensions: EXTENSIONS,
        tags: &[
            FileTag {
                pattern: "*.server.clpp",
                script_kind: "server",
                rojo_class: "Script",
            },
            FileTag {
                pattern: "*.client.clpp",
                script_kind: "client",
                rojo_class: "LocalScript",
            },
            FileTag {
                pattern: "*.plugin.clpp",
                script_kind: "plugin",
                rojo_class: "Script",
            },
            FileTag {
                pattern: "*.clp",
                script_kind: "module",
                rojo_class: "ModuleScript",
            },
            FileTag {
                pattern: "*.clh",
                script_kind: "header",
                rojo_class: "ModuleScript",
            },
        ],
        builtins: crate::builtins::names(),
        operators: &[
            OperatorMap {
                clpp: ".",
                luau: ". / :",
                meaning: "property / instance method",
            },
            OperatorMap {
                clpp: ":",
                luau: "pcall",
                meaning: "type annotation / protected call",
            },
            OperatorMap {
                clpp: "::",
                luau: ". / :",
                meaning: "static scope / manual Connect",
            },
            OperatorMap {
                clpp: ".:",
                luau: "..",
                meaning: "string concat",
            },
            OperatorMap {
                clpp: "~>",
                luau: "janitor:Add(Connect)",
                meaning: "auto-cleanup connection",
            },
            OperatorMap {
                clpp: "%",
                luau: "%",
                meaning: "modulo",
            },
            OperatorMap {
                clpp: "**",
                luau: "^",
                meaning: "power",
            },
            OperatorMap {
                clpp: "?.",
                luau: "if _t then",
                meaning: "optional chain",
            },
            OperatorMap {
                clpp: "??",
                luau: "if _t ~= nil",
                meaning: "null coalesce",
            },
            OperatorMap {
                clpp: "?",
                luau: "if-expr",
                meaning: "ternary",
            },
        ],
        io: &[
            IoMap {
                clpp: "post",
                luau: "print",
            },
            IoMap {
                clpp: "warn",
                luau: "warn",
            },
            IoMap {
                clpp: "report",
                luau: "error",
            },
            IoMap {
                clpp: "to_string",
                luau: "tostring",
            },
            IoMap {
                clpp: "to_number",
                luau: "tonumber",
            },
            IoMap {
                clpp: "to_bool",
                luau: "not not",
            },
        ],
    }
}
