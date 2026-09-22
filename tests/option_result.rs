//! Option / Result beyond smoke (RFC 0012 / 0013).

use clpp::compile::compile_artifact_source;
use std::path::Path;

fn compile(src: &str) -> (bool, String, Vec<String>, Vec<String>) {
    let art = compile_artifact_source(src, Path::new("option_result.clpp"), None).expect("compile");
    let codes: Vec<String> = art.diagnostics.iter().filter_map(|d| d.code.clone()).collect();
    let msgs: Vec<String> = art.diagnostics.iter().map(|d| d.message.clone()).collect();
    (art.ok, art.luau, codes, msgs)
}

#[test]
fn ok_err_emit_and_try() {
    let src = r#"
Result<int, string> f(bool ok) {
  if (ok) { return Ok(1); }
  return Err("no");
}
int g(bool ok) {
  return f(ok)?;
}
"#;
    let (ok, luau, codes, msgs) = compile(src);
    assert!(ok, "{codes:?} {msgs:?}");
    assert!(luau.contains("ok") || luau.contains("{"), "{luau}");
}

#[test]
fn result_assign_to_plain_is_error() {
    let src = r#"
void bad() {
  Result<int, string> r = Ok(1);
  int x = r;
}
"#;
    let (ok, _, codes, msgs) = compile(src);
    assert!(!ok, "{msgs:?}");
    assert!(
        codes.iter().any(|c| c == "CLPP0202"),
        "expected CLPP0202, got {codes:?} {msgs:?}"
    );
}

#[test]
fn match_result_exhaustive_ok() {
    let src = r#"
int unwrap(Result<int, string> r) {
  match (r) {
    Ok v => { return v; },
    Err e => { return 0; },
  }
}
"#;
    let (ok, luau, codes, msgs) = compile(src);
    assert!(ok, "{codes:?} {msgs:?}\n{luau}");
    assert!(luau.contains(".ok") || luau.contains("ok"), "{luau}");
}

#[test]
fn match_result_non_exhaustive() {
    let src = r#"
int bad(Result<int, string> r) {
  match (r) {
    Ok v => { return v; },
  }
}
"#;
    let (ok, _, codes, msgs) = compile(src);
    assert!(!ok, "{msgs:?}");
    assert!(
        codes.iter().any(|c| c == "CLPP1102"),
        "expected CLPP1102, got {codes:?} {msgs:?}"
    );
}

#[test]
fn try_on_non_result() {
    let src = r#"
int bad(int x) {
  return x?;
}
"#;
    let (ok, _, codes, msgs) = compile(src);
    assert!(!ok, "{msgs:?}");
    assert!(
        codes.iter().any(|c| c == "CLPP1101"),
        "expected CLPP1101, got {codes:?} {msgs:?}"
    );
}

#[test]
fn option_some_none_match() {
    let src = r#"
int use(optional<int> o) {
  match (o) {
    Some v => { return v; },
    None => { return 0; },
  }
}
"#;
    let (ok, _, codes, msgs) = compile(src);
    assert!(ok, "{codes:?} {msgs:?}");
}

#[test]
fn ternary_and_try_do_not_conflict() {
    let ternary_only = r#"
int mix(int cond) {
  int a = cond ? 2 : 3;
  return cond ? a : 0;
}
"#;
    let (ok, luau, codes, msgs) = compile(ternary_only);
    assert!(ok, "ternary: {codes:?} {msgs:?}\n{luau}");

    let try_only = r#"
int unwrap(Result<int, string> r) {
  return r?;
}
"#;
    let (ok, luau, codes, msgs) = compile(try_only);
    assert!(ok, "try: {codes:?} {msgs:?}\n{luau}");

    let ret_ternary = r#"
int mix(int cond) {
  int a = 1;
  int b = 2;
  return cond ? a : b;
}
"#;
    let (ok, luau, codes, msgs) = compile(ret_ternary);
    assert!(ok, "ret_ternary: {codes:?} {msgs:?}\n{luau}");

    let both = r#"
int mix(int cond, Result<int, string> r) {
  int a = cond ? 2 : 3;
  int b = r?;
  return cond ? a : b;
}
"#;
    let (ok, luau, codes, msgs) = compile(both);
    assert!(ok, "both: {codes:?} {msgs:?}\n{luau}");
    assert!(luau.contains("if"), "expected ternary emit, got:\n{luau}");
}