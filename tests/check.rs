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
fn rejects_func_capture_list() {
    let err = compile("void F() { auto cb = func [](int n) { return; }; }").unwrap_err();
    assert!(
        err.contains("expected") || err.contains("func"),
        "got: {err}"
    );
}

#[test]
fn accepts_func_callback() {
    let luau = compile("void F() { auto cb = func (int n) { post(n); }; }").expect("func callback");
    assert!(luau.contains("function(n: number)"), "got: {luau}");
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
    )
    .expect("artifact");
    assert!(!art.ok);
    assert!(
        art.diagnostics.iter().any(|d| d.message.contains("cannot initialize")),
        "got {:?}",
        art.diagnostics
    );
    assert!(art.diagnostics[0].line >= 1);
}

#[test]
fn diagnostic_const_reassign_and_include_keep_source_line() {
    let src = "#include <clpp/roblox.clh>\n\nvoid F() {\n\tconst int n = 1;\n\tn = 2;\n}\n";
    let art = compile_artifact_source(src, Path::new("const.clp"), None).expect("artifact");
    assert!(!art.ok);
    let found = art
        .diagnostics
        .iter()
        .find(|d| d.message.contains("const"))
        .expect("const diagnostic");
    assert!(
        found.line >= 5,
        "expected error on n = 2 (line 5+), got line {}",
        found.line
    );
}

#[test]
fn diagnostic_wrong_type_after_includes() {
    let src = "#include <clpp/roblox.clh>\n\nvoid F() {\n\tint n = \"x\";\n}\n";
    let art = compile_artifact_source(src, Path::new("type.clp"), None).expect("artifact");
    assert!(!art.ok);
    let found = art
        .diagnostics
        .iter()
        .find(|d| d.message.contains("cannot initialize"))
        .expect("type diagnostic");
    assert!(
        found.line >= 4,
        "expected error on int n = \"x\" (line 4), got line {}",
        found.line
    );
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
    players.PlayerAdded~>Once(func (Player* p) {
        return;
    });
}
"#,
    )
    .expect("cleanup after . and ~>");
    assert!(luau.contains(":Once(") || luau.contains("Once"), "got: {luau}");
}

#[test]
fn accessors_dot_colon_scope_and_protected() {
    let luau = compile(
        r#"
void F(Player* player) {
    Players* players = GetService<Players>();
    player.Name = "Kartz";
    player.Kick();
    Instance* child = static_cast<Instance>(player:FindFirstChild("x"));
    players.PlayerAdded::Connect(func (Player* p) {
        post(p.Name);
    });
}
"#,
    )
    .expect("accessors");
    assert!(luau.contains("player.Name = \"Kartz\""), "got: {luau}");
    assert!(luau.contains("player:Kick()"), "got: {luau}");
    assert!(luau.contains("pcall"), "protected : call, got: {luau}");
    assert!(luau.contains("FindFirstChild"), "got: {luau}");
    assert!(luau.contains("PlayerAdded:Connect"), "got: {luau}");
    assert!(!luau.contains("janitor:Add") && !luau.contains("__janitor:Add"), "manual ::Connect, got: {luau}");
}

#[test]
fn postfix_type_annotation() {
    let luau = compile(
        r#"
void F() {
    age: int = 10;
    post(age);
}
"#,
    )
    .expect("postfix type");
    assert!(luau.contains("age"), "got: {luau}");
    assert!(luau.contains("10"), "got: {luau}");
}

#[test]
fn pragma_nostrict_emits_nonstrict() {
    let luau = compile("#pragma nostrict\nvoid init() {}\n").expect("nostrict");
    assert!(luau.contains("--!nonstrict"));
    assert!(!luau.contains("--!strict\n"));
}

#[test]
fn to_string_number_bool_emit_luau_conversions() {
    let luau = compile(
        r#"
void F() {
    string s = to_string(50);
    float n = to_number("12");
    bool ok = to_bool(s);
    post(s, n, ok);
}
"#,
    )
    .expect("conversions");
    assert!(luau.contains("tostring(50)"), "got: {luau}");
    assert!(luau.contains("tonumber(\"12\")"), "got: {luau}");
    assert!(luau.contains("not not (s)"), "got: {luau}");
    assert!(!luau.contains("to_string("));
    assert!(!luau.contains("to_number("));
    assert!(!luau.contains("to_bool("));
}

#[test]
fn at_this_and_at_field_emit_self() {
    let luau = compile(
        r#"
struct Service {
    Janitor* janitor;
    void Tick();
};

void Service::Tick() {
    @janitor.Add(@this, "Destroy");
    this.janitor.Cleanup();
}
"#,
    )
    .expect("@this emit");
    assert!(luau.contains("self.janitor:Add(self, \"Destroy\")"), "got: {luau}");
    assert!(luau.contains("self.janitor:Cleanup()"), "got: {luau}");
    assert!(!luau.contains("@this"), "got: {luau}");
}

#[test]
fn at_this_outside_method_fails() {
    let err = compile(
        r#"
void init() {
    @this;
}
"#,
    )
    .unwrap_err();
    assert!(
        err.contains("@this") && err.to_lowercase().contains("class::method"),
        "got: {err}"
    );
}

#[test]
fn this_alias_still_emits_self() {
    let luau = compile(
        r#"
struct Service {
    int coins;
    void Tick();
};

void Service::Tick() {
    this;
    @coins = 1;
}
"#,
    )
    .expect("this alias");
    assert!(luau.contains("self"), "got: {luau}");
    assert!(luau.contains("self.coins = 1"), "got: {luau}");
}

#[test]
fn at_unknown_field_fails() {
    let err = compile(
        r#"
struct Service {
    int coins;
    void Tick();
};

void Service::Tick() {
    @nope = 1;
}
"#,
    )
    .unwrap_err();
    assert!(err.contains("@nope"), "got: {err}");
}

#[test]
fn missing_return_value_is_an_error() {
    let err = compile(
        r#"
Player Find() {
    return;
}
"#,
    )
    .unwrap_err();
    assert!(err.contains("return"), "got: {err}");
}

#[test]
fn pcall_destructure_and_multi_return() {
    let luau = compile(
        r#"
void F() {
    auto [success, erromessage] = pcall(func () {
        return true, "Deu erro prq sim";
    });
    post(success);
}
"#,
    )
    .expect("pcall");
    assert!(luau.contains("local success, erromessage"), "got: {luau}");
    assert!(luau.contains("return true, \"Deu erro prq sim\""), "got: {luau}");
}

#[test]
fn comments_emit_as_luau() {
    let luau = compile(
        r#"
// setup the service
void F() {
    post("ok"); // greet
}
"#,
    )
    .expect("comments");
    assert!(luau.contains("-- setup the service"), "got: {luau}");
    assert!(luau.contains("-- greet"), "got: {luau}");
}

#[test]
fn extra_semicolon_after_for_is_ok() {
    let luau = compile(
        r#"
void F() {
    for (int i = 0; i < 2; i++) {
        post(i);
    };
}
"#,
    )
    .expect("extra semicolon");
    assert!(
        luau.contains("while") || luau.contains("for"),
        "got: {luau}"
    );
}

#[test]
fn header_emits_type_and_ctor() {
    let art = compile_artifact_source(
        r#"
struct Leaderstats {
    Janitor janitor = new Janitor();
    Player PegarJogador(string NomeDoJogador);
};
"#,
        Path::new("Leaderstats.clh"),
        None,
    )
    .expect("header");
    assert!(art.ok, "{:?}", art.error);
    assert!(art.luau.contains("export type Leaderstats"), "got: {}", art.luau);
    assert!(art.luau.contains("PegarJogador"), "got: {}", art.luau);
    assert!(art.luau.contains("const function Leaderstats"), "got: {}", art.luau);
}

#[test]
fn header_void_methods_export_unit_return() {
    let art = compile_artifact_source(
        r#"
struct CurrenciesClient {
    const void PathUpdate(Instance inst, double NewValue);
    const void init();
};
"#,
        Path::new("CurrenciesClient.clh"),
        None,
    )
    .expect("header");
    assert!(art.ok, "{:?}", art.error);
    assert!(
        art.luau.contains(
            "PathUpdate: (self: CurrenciesClient, inst: Instance, NewValue: number) -> ()"
        ),
        "got: {}",
        art.luau
    );
    assert!(
        art.luau.contains("init: (self: CurrenciesClient) -> ()"),
        "got: {}",
        art.luau
    );
}

#[test]
fn observable_initial_value_is_marked() {
    let luau = compile(
        r#"
void F() {
    observable int moeda = 10;
    moeda = 150;
    moeda.OnChange(func (int NovoValorDaMoeda) {
        post(NovoValorDaMoeda);
    });
}
"#,
    )
    .expect("observable");
    assert!(luau.contains("initial value"), "got: {luau}");
    assert!(luau.contains("__clpp_obs"), "got: {luau}");
    assert!(luau.contains("moeda.Value = 150"), "got: {luau}");
}

#[test]
fn quoted_include_searches_src() {
    let root = std::env::temp_dir().join(format!("clpp-inc-{}", std::process::id()));
    let data_dir = root.join("src").join("ReplicatedStorage").join("Shareds");
    std::fs::create_dir_all(&data_dir).unwrap();
    std::fs::write(data_dir.join("PlayerData.clp"), "const int kCoins = 1;\n").unwrap();
    let server = root.join("src").join("Server");
    std::fs::create_dir_all(&server).unwrap();
    let from = server.join("Main.server.clpp");
    let src = r#"
#include "src/ReplicatedStorage/Shareds/PlayerData.clp"
void F() { post("ok"); }
"#;
    let art = compile_artifact_source(src, &from, None).expect("include");
    let _ = std::fs::remove_dir_all(&root);
    assert!(art.ok, "{:?}", art.error);
    assert!(
        art.luau.contains("require") || art.luau.contains("kCoins") || art.luau.contains("post"),
        "got: {}",
        art.luau
    );
}

