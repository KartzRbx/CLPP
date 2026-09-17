use clpp::compile::{compile_artifact_source, compile_source};
use std::path::Path;

fn compile(src: &str) -> Result<String, String> {
    compile_source(src, Path::new("check.clp")).map_err(|e| format!("{e:#}"))
}

#[test]
fn rejects_string_assigned_to_const_int() {
    let err = compile("void F() { const int coins = \"Value\"; }").unwrap_err();
    assert!(
        err.contains("cannot initialize") && err.contains("string"),
        "got: {err}"
    );
}

#[test]
fn rejects_missing_semicolon() {
    let err = compile("void F() {\n\tconst int coins = 4\n\treturn;\n}").unwrap_err();
    assert!(
        err.to_lowercase().contains("missing") || err.contains(';') || err.contains("expected"),
        "got: {err}"
    );
}

#[test]
fn rejects_const_int_without_name() {
    let err = compile("void F() { const int = \"Value\"; }").unwrap_err();
    assert!(
        err.contains("name") || err.contains("ident") || err.contains("expected"),
        "got: {err}"
    );
}

#[test]
fn accepts_single_quotes_and_templates() {
    let luau = compile(
        r#"
void F(Player* player) {
    string a = 'hello';
    string b = `PlayerName is {player.Name}`;
    string c = `PlayerName is `, player.Name, `.`;
    post(a .: b .: c);
}
"#,
    )
    .expect("compile strings");
    assert!(luau.contains("\"hello\""));
    assert!(luau.contains("PlayerName is "));
    assert!(luau.contains("player.Name") || luau.contains("tostring"));
}

#[test]
fn json_diagnostics_on_type_error() {
    let art = compile_artifact_source(
        "void F() { int n = \"x\"; }",
        Path::new("bad.clp"),
        None,
    );
    assert!(art.is_err());
}

#[test]
fn range_for_in_names_the_collection() {
    let luau = compile(
        r#"
void F() {
    array<int> xs = {1, 2};
    for (int x in xs) {
        post(x);
    }
    for (int y : xs) {
        post(y);
    }
}
"#,
    )
    .expect("range-for in");
    assert!(luau.contains("for _, x in xs do"));
    assert!(luau.contains("for _, y in xs do"));
}

#[test]
fn pragma_once_is_valid_in_headers() {
    let luau = compile_source(
        "#pragma once\nstruct Foo { int Coins = 0; };\n",
        Path::new("Foo.clh"),
    )
    .expect("pragma once header");
    assert!(luau.contains("Coins = 0"));
    assert!(!luau.contains("#pragma"));
}

#[test]
fn pragma_directives_emit_luau_comments() {
    let luau = compile(
        r#"
#pragma strict
#pragma native
#pragma optimize 2
void init() {}
"#,
    )
    .expect("pragma emit");
    let head = luau.lines().take(6).collect::<Vec<_>>().join("\n");
    assert!(head.contains("--!strict"), "got: {head}");
    assert!(head.contains("--!native"), "got: {head}");
    assert!(head.contains("--!optimize 2"), "got: {head}");
}

#[test]
fn janitor_cleanup_after_scope_member() {
    let luau = compile(
        r#"
void F() {
    Players* players = GetService<Players>();
    players::PlayerAdded~>Once(func [](Player* p) {
        return;
    });
}
"#,
    )
    .expect("cleanup after :: and .");
    assert!(luau.contains(":Once(") || luau.contains("Once"), "got: {luau}");
}

#[test]
fn pragma_nostrict_emits_nonstrict() {
    let luau = compile("#pragma nostrict\nvoid init() {}\n").expect("nostrict");
    assert!(luau.contains("--!nonstrict"));
    assert!(!luau.contains("--!strict\n"));
}
