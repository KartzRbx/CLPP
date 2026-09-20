//! Named lints, separate from hard errors. `#pragma nolint Name` maps to `--!nolint`.

use crate::support::CompileDiagnostic;

#[derive(Clone, Copy)]
pub struct Lint {
    pub name: &'static str,
    pub level: &'static str,
    pub help: &'static str,
}

pub const UNUSED_LOCAL: Lint = Lint {
    name: "UnusedLocal",
    level: "warning",
    help: "prefix with `_` or remove the binding",
};

pub const SHADOWING: Lint = Lint {
    name: "Shadowing",
    level: "warning",
    help: "rename the inner binding",
};

pub fn all() -> &'static [Lint] {
    &[UNUSED_LOCAL, SHADOWING]
}

pub fn to_diag(lint: Lint, line: usize, column: usize, detail: impl std::fmt::Display) -> CompileDiagnostic {
    CompileDiagnostic {
        message: format!("warning[{}]: {detail}", lint.name),
        line: line.max(1),
        column: column.max(1),
        severity: lint.level.into(),
        code: Some(lint.name.into()),
        help: Some(lint.help.into()),
    }
}

pub fn luau_nolint(names: &[String]) -> Option<String> {
    if names.is_empty() {
        None
    } else {
        Some(format!("--!nolint {}", names.join(", ")))
    }
}
