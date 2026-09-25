//! Context and parallel safety. The host passes [RunContext]; this crate emits
//! the diagnostics the LSP underlines.

mod rich;

pub use rich::{rich_diagnostics, ClppDiagnostic};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RunContext {
    Client,
    Server,
    Module,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Diagnostic {
    pub code: &'static str,
    pub message: String,
    /// 1-based.
    pub line: usize,
    /// Byte offset of the line in `source`.
    pub start: usize,
    pub len: usize,
}

pub fn line_span(source: &str, line_no: usize) -> (usize, usize) {
    let start: usize = source.lines().take(line_no.saturating_sub(1)).map(|l| l.len() + 1).sum();
    let len = source.lines().nth(line_no.saturating_sub(1)).map(|l| l.len()).unwrap_or(0);
    (start, len)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Check {
    pub diagnostics: Vec<Diagnostic>,
}

impl Check {
    pub fn ok(&self) -> bool {
        self.diagnostics.is_empty()
    }
}

/// `CLUAU_AUTH001` — `@server` item used while the file is Client.
/// `CLUAU_PAR001` — assignment inside an `@parallel` region.
pub fn check(source: &str, ctx: RunContext) -> Check {
    let lines: Vec<&str> = source.lines().collect();
    let mut diagnostics = Vec::new();
    let server_fns = server_function_names(&lines);

    let mut parallel_depth: i32 = 0;
    let mut arm_parallel = false;
    for (idx, line) in lines.iter().enumerate() {
        let line_no = idx + 1;
        let trimmed = line.trim();

        if ctx == RunContext::Client && (trimmed == "@server" || trimmed.starts_with("@server ")) {
            push(&mut diagnostics, source, line_no, "CLUAU_AUTH001", "server authority is not allowed in a Client context".into());
        }
        if ctx == RunContext::Server && (trimmed == "@client" || trimmed.starts_with("@client ")) {
            push(&mut diagnostics, source, line_no, "CLUAU_AUTH001", "client-only item is not allowed in a Server context".into());
        }

        if ctx == RunContext::Client {
            for name in &server_fns {
                if calls(trimmed, name) {
                    push(&mut diagnostics, source, line_no, "CLUAU_AUTH001", format!("call to server function `{name}` from a Client context"));
                }
            }
        }

        if trimmed == "@parallel" || trimmed.starts_with("@parallel ") || trimmed.starts_with("@parallel{") {
            arm_parallel = true;
        }
        if arm_parallel && trimmed.contains('{') {
            parallel_depth += 1;
            arm_parallel = false;
        } else if parallel_depth > 0 {
            parallel_depth += brace_delta(trimmed);
        }
        if parallel_depth > 0 && is_mutation(trimmed) {
            push(&mut diagnostics, source, line_no, "CLUAU_PAR001", "mutation is not allowed under @parallel".into());
        }
        if parallel_depth < 0 {
            parallel_depth = 0;
        }
    }

    Check { diagnostics }
}

fn push(out: &mut Vec<Diagnostic>, source: &str, line: usize, code: &'static str, message: String) {
    let (start, len) = line_span(source, line);
    out.push(Diagnostic { code, message, line, start, len });
}

pub fn lint(source: &str) -> Vec<Diagnostic> {
    let mut out = Vec::new();
    let has_disconnect = source.contains("Disconnect");
    for (idx, line) in source.lines().enumerate() {
        let line_no = idx + 1;
        let t = line.trim();
        if t.contains("ChildAdded") && !has_disconnect {
            push(&mut out, source, line_no, "CLUAU_LINT001", "ChildAdded connection is never disconnected".into());
        }
        if t.contains("RenderStepped") {
            push(&mut out, source, line_no, "CLUAU_LINT002", "synchronous work on RenderStepped".into());
        }
        if t.contains("spawn(") && !t.contains("task.spawn") && !t.contains("task::spawn") {
            push(&mut out, source, line_no, "CLUAU_LINT003", "use task.spawn instead of legacy spawn".into());
        }
    }
    out
}

fn server_function_names(lines: &[&str]) -> Vec<String> {
    let mut names = Vec::new();
    for (idx, line) in lines.iter().enumerate() {
        if line.trim() != "@server" {
            continue;
        }
        if let Some(name) = lines.get(idx + 1).and_then(|next| fn_name(next.trim())) {
            names.push(name);
        }
    }
    names
}

fn fn_name(line: &str) -> Option<String> {
    let rest = line.strip_prefix("void ").or_else(|| line.strip_prefix("fn "))?;
    let name = rest.split('(').next()?.trim();
    if name.is_empty() || name.contains(' ') {
        None
    } else {
        Some(name.to_string())
    }
}

fn calls(line: &str, name: &str) -> bool {
    line.split(|c: char| !c.is_ascii_alphanumeric() && c != '_')
        .any(|part| part == name)
        && line.contains('(')
        && !line.trim_start().starts_with("void ")
}

fn is_mutation(line: &str) -> bool {
    let mut rest = line;
    while let Some(i) = rest.find('=') {
        let after = rest.as_bytes().get(i + 1).copied();
        let before = if i == 0 { None } else { rest.as_bytes().get(i - 1).copied() };
        let cmp = matches!(after, Some(b'='))
            || matches!(before, Some(b'=') | Some(b'!') | Some(b'<') | Some(b'>'));
        if !cmp {
            return true;
        }
        rest = &rest[i + 1..];
    }
    false
}

fn brace_delta(line: &str) -> i32 {
    let open = line.chars().filter(|c| *c == '{').count() as i32;
    let close = line.chars().filter(|c| *c == '}').count() as i32;
    open - close
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn client_calling_server_is_auth() {
        let src = "@server\nvoid Save();\nvoid init() {\n    Save();\n}\n";
        let check = check(src, RunContext::Client);
        assert!(check.diagnostics.iter().any(|d| d.code == "CLUAU_AUTH001" && d.line == 4));
    }

    #[test]
    fn parallel_mutation_is_par() {
        let src = "@parallel\nvoid tick() {\n    coins = 1;\n}\n";
        let check = check(src, RunContext::Server);
        assert!(check.diagnostics.iter().any(|d| d.code == "CLUAU_PAR001"));
    }

    #[test]
    fn server_file_without_parallel_write_is_clean() {
        let src = "@server\nvoid Save();\nvoid init() {\n    Save();\n}\n";
        assert!(check(src, RunContext::Server).ok());
    }
}
