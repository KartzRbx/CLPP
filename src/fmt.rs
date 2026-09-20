//! Indent-only formatter for CL++ source. Does not rewrite syntax.

pub fn format_source(source: &str) -> String {
    let mut out = String::new();
    let mut indent = 0i32;
    let mut in_string = false;
    let mut quote = '\0';
    for (i, line) in source.lines().enumerate() {
        if i > 0 {
            out.push('\n');
        }
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }
        let (code, comment) = split_comment(trimmed);
        let mut lead = 0i32;
        let mut trail = 0i32;
        if !in_string {
            let t = code.trim();
            if t.starts_with('}') || t.starts_with("} else") || t.starts_with("} else if") {
                lead = -1;
            }
            trail = brace_delta(t);
        }
        indent = (indent + lead).max(0);
        for _ in 0..indent {
            out.push('\t');
        }
        out.push_str(code.trim());
        if let Some(c) = comment {
            if !code.trim().is_empty() {
                out.push(' ');
            }
            out.push_str("// ");
            out.push_str(c.trim());
        }
        indent = (indent + trail).max(0);
        update_string_state(line, &mut in_string, &mut quote);
    }
    out = rewrite_receiver_style(&out);
    if !source.ends_with('\n') && !out.ends_with('\n') {
        out.push('\n');
    }
    out
}

fn rewrite_receiver_style(source: &str) -> String {
    let s = source.replace("@this::", "@");
    let mut out = String::with_capacity(s.len());
    let chars: Vec<char> = s.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        if chars[i] == 't'
            && chars.get(i..i + 5) == Some(&['t', 'h', 'i', 's', '.'])
            && (i == 0 || !(chars[i - 1].is_ascii_alphanumeric() || chars[i - 1] == '_' || chars[i - 1] == '@'))
        {
            out.push_str("@this.");
            i += 5;
            continue;
        }
        out.push(chars[i]);
        i += 1;
    }
    out
}

fn split_comment(line: &str) -> (&str, Option<&str>) {
    if let Some(idx) = find_line_comment(line) {
        (&line[..idx], Some(&line[idx + 2..]))
    } else {
        (line, None)
    }
}

fn find_line_comment(line: &str) -> Option<usize> {
    let mut in_string = false;
    let mut quote = '\0';
    let chars: Vec<char> = line.chars().collect();
    let mut i = 0;
    while i + 1 < chars.len() {
        let c = chars[i];
        if in_string {
            if c == '\\' {
                i += 2;
                continue;
            }
            if c == quote {
                in_string = false;
            }
            i += 1;
            continue;
        }
        if c == '"' || c == '\'' || c == '`' {
            in_string = true;
            quote = c;
            i += 1;
            continue;
        }
        if c == '/' && chars[i + 1] == '/' {
            return Some(i);
        }
        i += 1;
    }
    None
}

fn brace_delta(code: &str) -> i32 {
    let mut n = 0i32;
    for c in code.chars() {
        if c == '{' {
            n += 1;
        } else if c == '}' {
            n -= 1;
        }
    }
    n
}

fn update_string_state(line: &str, in_string: &mut bool, quote: &mut char) {
    let chars: Vec<char> = line.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        let c = chars[i];
        if *in_string {
            if c == '\\' {
                i += 2;
                continue;
            }
            if c == *quote {
                *in_string = false;
            }
            i += 1;
            continue;
        }
        if c == '"' || c == '\'' || c == '`' {
            *in_string = true;
            *quote = c;
        }
        i += 1;
    }
}
