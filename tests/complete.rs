use clpp::analysis::{complete_request, hover_request, symbols_request, PositionRequest};
use clpp::compile::{compile_artifact_source, compile_source};
use std::path::Path;

fn pos(source: &str, needle: &str) -> PositionRequest {
    let idx = source.find(needle).expect(needle);
    let before = &source[..idx + needle.len()];
    let line = before.lines().count();
    let column = before.lines().last().unwrap().len() + 1;
    PositionRequest {
        source: source.to_string(),
        file_name: "ide.clpp".into(),
        line,
        column,
    }
}

#[test]
fn completes_local_and_player_entered() {
    let src = r#"
void PlayerEntered(Player player) {
    post(player.Name);
}

void init() {
    
}
"#;
    let items = complete_request(&PositionRequest {
        source: src.into(),
        file_name: "ide.clpp".into(),
        line: 7,
        column: 5,
    })
    .items;
    assert!(
        items.iter().any(|i| i.label == "PlayerEntered" || i.label.contains("PlayerEntered")),
        "{:?}",
        items.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}

#[test]
fn completes_range_for_player_members() {
    let src = r#"
void init() {
    Players players = GetService<Players>();
    for (Player jogador in players.GetPlayers()) {
        jogador.
    }
}
"#;
    let items = complete_request(&pos(src, "        jogador.")).items;
    assert!(
        items.iter().any(|i| i.label == "Name"),
        "{:?}",
        items.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}

#[test]
fn completes_struct_method_and_at_field() {
    let src = r#"
struct LeaderstatsServer {
    Janitor janitor;
    void PlayerEntered(Player player);
};

void LeaderstatsServer::PlayerEntered(Player player) {
    @janitor;
}
"#;
    let items = complete_request(&pos(src, "    @")).items;
    assert!(items.iter().any(|i| i.label == "@this"), "{:?}", items);
    assert!(
        items.iter().any(|i| i.label == "@janitor" || i.label.contains("janitor")),
        "{:?}",
        items.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}

#[test]
fn hover_builtin_post() {
    let src = "void init() {\n    post(\"hi\");\n}\n";
    let hover = hover_request(&pos(src, "post")).hover.expect("hover");
    assert!(hover.contents.contains("print"), "{}", hover.contents);
}

#[test]
fn outline_lists_free_function() {
    let src = "void PlayerEntered(Player player) {\n    return;\n}\n";
    let symbols = symbols_request(&PositionRequest {
        source: src.into(),
        file_name: "mod.clp".into(),
        line: 1,
        column: 1,
    })
    .symbols;
    assert!(
        symbols.iter().any(|s| s.name.contains("PlayerEntered")),
        "{:?}",
        symbols
    );
}

#[test]
fn unknown_ident_is_error() {
    let art = compile_artifact_source(
        "void init() { foo(); }",
        Path::new("bad.clp"),
        None,
    )
    .expect("artifact");
    assert!(!art.ok);
    assert!(
        art.diagnostics.iter().any(|d| d.message.contains("unknown identifier")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn arity_mismatch_is_error() {
    let src = r#"
void Greet(string name) {
    post(name);
}
void init() {
    Greet();
}
"#;
    let art = compile_artifact_source(src, Path::new("arity.clp"), None).expect("artifact");
    assert!(!art.ok);
    assert!(
        art.diagnostics.iter().any(|d| d.message.contains("argument")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn using_is_error() {
    let art = compile_artifact_source(
        "using namespace std;\nvoid init() { return; }\n",
        Path::new("using.clp"),
        None,
    )
    .expect("artifact");
    assert!(!art.ok);
    assert!(
        art.diagnostics.iter().any(|d| d.message.contains("using") || d.code.as_deref() == Some("CLPP0301")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn comments_stay_near_source_line() {
    let luau = compile_source(
        "// keep me\nvoid init() {\n    post(1);\n}\n",
        Path::new("cmt.clp"),
    )
    .expect("compile");
    let keep = luau.find("-- keep me").expect("comment");
    let compiled = luau.find("Compiled by CL++").unwrap_or(0);
    assert!(keep > compiled, "{luau}");
}

#[test]
fn range_for_typed_compiles() {
    let luau = compile_source(
        r#"
void init() {
    Players players = GetService<Players>();
    for (Player jogador in players.GetPlayers()) {
        post(jogador.Name);
    }
}
"#,
        Path::new("for.clp"),
    )
    .expect("compile");
    assert!(luau.contains("for _, jogador in"), "{luau}");
}
