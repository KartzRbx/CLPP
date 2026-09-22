//! Stable diagnostic codes (CLPP####) plus explain text.

use crate::support::CompileDiagnostic;

#[derive(Clone, Copy)]
pub struct Code {
    pub id: &'static str,
    pub title: &'static str,
    pub help: &'static str,
}

pub const CLPP0101: Code = Code {
    id: "CLPP0101",
    title: "method called without receiver",
    help: "prefix the call with `@`, e.g. `@Name(args)`",
};
pub const CLPP0102: Code = Code {
    id: "CLPP0102",
    title: "field used without receiver",
    help: "prefix the field with `@`, e.g. `@name`",
};
pub const CLPP0201: Code = Code {
    id: "CLPP0201",
    title: "optional assigned to a non-optional type",
    help: "use optional<T>, guard, assert, or static_cast",
};
pub const CLPP0301: Code = Code {
    id: "CLPP0301",
    title: "construct is not in CL++",
    help: "remove it or rewrite using a struct / module file",
};
pub const CLPP0401: Code = Code {
    id: "CLPP0401",
    title: "private member accessed outside the struct",
    help: "keep the access inside Class::Method or make the member public",
};
pub const CLPP0402: Code = Code {
    id: "CLPP0402",
    title: "continue outside a loop",
    help: "use continue only inside while / for / for-each",
};
pub const CLPP0501: Code = Code {
    id: "CLPP0501",
    title: "match/switch is not exhaustive for this enum",
    help: "cover every variant or add a default / `_` arm",
};
pub const CLPP0601: Code = Code {
    id: "CLPP0601",
    title: "method declared but not defined",
    help: "add Class::Method in this translation unit",
};
pub const CLPP0602: Code = Code {
    id: "CLPP0602",
    title: "method defined but not declared",
    help: "declare the method inside the struct",
};
pub const CLPP0603: Code = Code {
    id: "CLPP0603",
    title: "definition signature differs from the declaration",
    help: "make parameters and return type match the struct prototype",
};
pub const CLPP0604: Code = Code {
    id: "CLPP0604",
    title: "unknown member",
    help: "check the spelling or include the header that declares the type",
};
pub const CLPP0605: Code = Code {
    id: "CLPP0605",
    title: "library type used without its header",
    help: "add `#include <clpp/libs/….clh>` for this struct",
};
pub const CLPP0701: Code = Code {
    id: "CLPP0701",
    title: "static_assert failed",
    help: "the condition must be a true compile-time constant",
};
pub const CLPP0801: Code = Code {
    id: "CLPP0801",
    title: "module import not found",
    help: "use a path the quoted-include resolver can find (.clh / .clpp next to this file)",
};
pub const CLPP0901: Code = Code {
    id: "CLPP0901",
    title: "generic type parameter bound violated",
    help: "pass a type that implements the bound, or add the missing members / widen the bound",
};

pub fn all() -> &'static [Code] {
    &[
        CLPP0101, CLPP0102, CLPP0201, CLPP0301, CLPP0401, CLPP0402, CLPP0501, CLPP0601, CLPP0602,
        CLPP0603, CLPP0604, CLPP0605, CLPP0701, CLPP0801, CLPP0901,
    ]
}

pub fn explain(id: &str) -> Option<&'static Code> {
    let id = id.trim().trim_start_matches("error[").trim_end_matches(']');
    all().iter().find(|c| c.id.eq_ignore_ascii_case(id))
}

pub fn message(code: Code, detail: impl std::fmt::Display) -> String {
    format!("error[{}]: {}: {detail}", code.id, code.title)
}

pub fn warning(code: Code, detail: impl std::fmt::Display) -> String {
    format!("warning[{}]: {}: {detail}", code.id, code.title)
}

pub fn diag(code: Code, line: usize, column: usize, detail: impl std::fmt::Display, severity: &str) -> CompileDiagnostic {
    let mut d = CompileDiagnostic {
        message: if severity == "warning" {
            warning(code, detail)
        } else {
            message(code, detail)
        },
        line: line.max(1),
        column: column.max(1),
        severity: severity.into(),
        code: None,
        help: None,
    };
    d.code = Some(code.id.into());
    d.help = Some(code.help.into());
    d
}

pub fn levenshtein(a: &str, b: &str) -> usize {
    let a: Vec<char> = a.chars().collect();
    let b: Vec<char> = b.chars().collect();
    let mut prev: Vec<usize> = (0..=b.len()).collect();
    let mut cur = vec![0; b.len() + 1];
    for (i, ca) in a.iter().enumerate() {
        cur[0] = i + 1;
        for (j, cb) in b.iter().enumerate() {
            let cost = if ca == cb { 0 } else { 1 };
            cur[j + 1] = (prev[j + 1] + 1).min(cur[j] + 1).min(prev[j] + cost);
        }
        std::mem::swap(&mut prev, &mut cur);
    }
    prev[b.len()]
}

pub fn did_you_mean<'a>(name: &str, candidates: impl IntoIterator<Item = &'a str>) -> Option<String> {
    let mut best: Option<(&str, usize)> = None;
    for c in candidates {
        let d = levenshtein(name, c);
        if d > 0 && d <= 2 && name.len().abs_diff(c.len()) <= 2 {
            if best.map(|(_, bd)| d < bd).unwrap_or(true) {
                best = Some((c, d));
            }
        }
    }
    best.map(|(c, _)| c.to_string())
}
