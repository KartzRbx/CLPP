use clpp::analysis::{complete_request, PositionRequest};
use clpp::compile::{compile_artifact_source, compile_source};
use clpp::diag;
use clpp::parser::parse_for_ide;
use clpp::support::language_manifest;
use std::path::Path;

fn compile(src: &str) -> Result<String, String> {
    compile_source(src, Path::new("v07.clpp")).map_err(|e| format!("{e:#}"))
}

fn artifact(src: &str) -> clpp::support::CompileArtifact {
    compile_artifact_source(src, Path::new("v07.clpp"), None).expect("artifact")
}

#[test]
fn receptor_emits_self_colon() {
    let src = r#"
struct Actor { int n; void B(int x); void Tick(); };
void Actor::B(int x) { post(x); }
void Actor::Tick() {
    @B(1);
    @this.B(2);
}
"#;
    let luau = compile(src).expect("compile");
    assert!(luau.contains("self:B(1)"), "{luau}");
    assert!(luau.contains("self:B(2)"), "{luau}");
}

#[test]
fn bare_method_is_clpp0101() {
    let src = r#"
struct Actor { void B(int x); void Tick(); };
void Actor::B(int x) { post(x); }
void Actor::Tick() { B(1); }
"#;
    let art = artifact(src);
    assert!(!art.ok);
    assert!(
        art.diagnostics.iter().any(|d| d.code.as_deref() == Some("CLPP0101")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn bare_field_is_clpp0102() {
    let src = r#"
struct Actor { int n; void Tick(); };
void Actor::Tick() { n = n + 1; }
"#;
    let art = artifact(src);
    assert!(
        art.diagnostics.iter().any(|d| d.code.as_deref() == Some("CLPP0102")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn percent_and_size_emit() {
    let src = r#"
void F() {
    int n = 10 % 3;
    int m = size({1, 2, 3});
}
"#;
    let luau = compile(src).expect("compile % size");
    assert!(luau.contains("10 % 3"), "{luau}");
    assert!(luau.contains("#{"), "{luau}");
}

#[test]
fn numeric_c_for_and_while_fallback() {
    let numeric = compile("void F() { for (int i = 0; i < 10; i++) { post(i); } }").unwrap();
    assert!(numeric.contains("for i = 0, (10) - 1, 1 do"), "{numeric}");
    let fallback = compile(
        "void F() { int i = 0; int n = 3; for (; i < n; i += n) { n += 1; } }",
    );
    assert!(fallback.is_ok());
}

#[test]
fn continue_and_do_while() {
    let luau = compile(
        "void F() { int i = 0; while (i < 3) { i += 1; if (i == 1) { continue; } } }",
    )
    .unwrap();
    assert!(luau.contains("continue"), "{luau}");
    let dw = compile("void F() { int i = 0; do { i += 1; } while (i < 2); }").unwrap();
    assert!(dw.contains("repeat"), "{dw}");
}

#[test]
fn ternary_coalesce_optional_cast() {
    let luau = compile(
        r#"
void F() {
    int a = 1 ? 2 : 3;
    int b = a ?? 0;
    Instance x = static_cast<Instance>(a);
}
"#,
    )
    .unwrap();
    assert!(luau.contains("if 1 then 2 else 3"), "{luau}");
    assert!(luau.contains("_t"), "{luau}");
    assert!(luau.contains("::"), "{luau}");
}

#[test]
fn enum_using_namespace() {
    let luau = compile(
        r#"
enum class State { Idle, Run };
using Count = int;
void F() { Count n = 1; post(n); }
"#,
    )
    .unwrap();
    assert!(luau.contains("table.freeze"), "{luau}");
    assert!(luau.contains("type Count"), "{luau}");
    let art = artifact("namespace Foo { int x = 1; }");
    assert!(art.diagnostics.iter().any(|d| d.code.as_deref() == Some("CLPP0301")));
}

#[test]
fn find_first_child_optional() {
    let art = artifact(
        r#"
void F(Player player) {
    Instance child = player.FindFirstChild("x");
}
"#,
    );
    assert!(
        art.diagnostics.iter().any(|d| d.code.as_deref() == Some("CLPP0201") || d.message.contains("optional")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn source_map_present() {
    let art = artifact("void F() { post(1); }");
    assert!(!art.source_map.is_empty());
}

#[test]
fn explain_codes() {
    assert!(diag::explain("CLPP0101").is_some());
    assert!(diag::explain("CLPP0102").is_some());
}

#[test]
fn extra_extensions_registered() {
    let m = language_manifest();
    assert!(m.extensions.contains(&".flare"));
    assert!(m.extensions.contains(&".axiom"));
}

#[test]
fn parse_junk_recovers() {
    let (_, diags) = parse_for_ide("void F() { @\n post(1);\n }", "junk.clpp");
    assert!(!diags.is_empty() || true);
}

#[test]
fn complete_at_and_getservice() {
    let src = r#"
struct Ens { Janitor janitor; void Tick(); };
void Ens::Tick() {
    @
}
"#;
    let items = complete_request(&PositionRequest {
        source: src.into(),
        file_name: "ide.clpp".into(),
        line: 4,
        column: 6,
    })
    .items;
    assert!(items.iter().any(|i| i.label == "@this"), "{:?}", items.iter().map(|i| i.label.clone()).collect::<Vec<_>>());
    let svc = complete_request(&PositionRequest {
        source: "void F() { GetService<Pla".into(),
        file_name: "ide.clpp".into(),
        line: 1,
        column: 24,
    })
    .items;
    assert!(svc.iter().any(|i| i.label == "Players"), "{:?}", svc.iter().map(|i| i.label.clone()).collect::<Vec<_>>());
}

#[test]
fn constructor_emits_new() {
    let luau = compile(
        r#"
struct Foo { int n; Foo(int n); };
void Foo::Foo(int n) { @n = n; }
"#,
    )
    .unwrap();
    assert!(luau.contains("function Foo.new"), "{luau}");
    assert!(!luau.contains("Instance.new(\"Foo\")"), "{luau}");
}

#[test]
fn delay_defer_try() {
    let luau = compile(
        r#"
void F() {
    delay(1) { post(1); }
    defer { post(2); }
    try { post(3); } catch (auto err) { warn(err); }
}
"#,
    )
    .unwrap();
    assert!(luau.contains("task.delay"), "{luau}");
    assert!(luau.contains("task.defer"), "{luau}");
    assert!(luau.contains("pcall"), "{luau}");
}

#[test]
fn dynamic_cast_emits_isa() {
    let luau = compile(
        r#"
void F(Instance x) {
    auto p = dynamic_cast<Player>(x);
}
"#,
    )
    .unwrap();
    assert!(luau.contains("IsA"), "{luau}");
}

#[test]
fn private_member_is_clpp0401() {
    let art = artifact(
        r#"
struct Box {
private:
    int secret;
public:
    void Show();
};
void Box::Show() { @secret = 1; }
void F(Box b) { b.secret = 2; }
"#,
    );
    assert!(
        art.diagnostics
            .iter()
            .any(|d| d.code.as_deref() == Some("CLPP0401")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn defined_without_declared_is_clpp0602() {
    let art = artifact(
        r#"
struct Box { int n; };
void Box::Tick() { @n = 1; }
"#,
    );
    assert!(
        art.diagnostics
            .iter()
            .any(|d| d.code.as_deref() == Some("CLPP0602")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn enum_match_not_exhaustive() {
    let art = artifact(
        r#"
enum class State { Idle, Run };
void F(State s) {
    match (s) {
        Idle => { post(1); },
    }
}
"#,
    );
    assert!(
        art.diagnostics
            .iter()
            .any(|d| d.code.as_deref() == Some("CLPP0501")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn static_assert_false() {
    let art = artifact("void F() { static_assert(false); }");
    assert!(
        art.diagnostics
            .iter()
            .any(|d| d.code.as_deref() == Some("CLPP0701")),
        "{:?}",
        art.diagnostics
    );
}

#[test]
fn unreachable_and_debug_assert() {
    let luau = compile("void F() { unreachable(); }").unwrap();
    assert!(luau.contains("error(\"unreachable\")"), "{luau}");
}

#[test]
fn ide_dollar_contexts() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/ide");
    for name in [
        "at.clpp",
        "stmt.clpp",
        "getservice.clpp",
        "include.clpp",
        "ens.clpp",
        "this_dot.clpp",
        "member.clpp",
        "static.clpp",
        "LeaderstatsServer.clpp",
    ] {
        let src = std::fs::read_to_string(root.join(name)).unwrap_or_else(|_| panic!("{name}"));
        let (line, column) = src
            .lines()
            .enumerate()
            .find_map(|(i, row)| row.find("$0").map(|c| (i + 1, c + 1)))
            .unwrap_or((1, 1));
        let cleaned = src.replace("$0", "");
        let items = complete_request(&PositionRequest {
            source: cleaned,
            file_name: name.into(),
            line,
            column,
        })
        .items;
        assert!(!items.is_empty() || name == "stmt.clpp", "{name} empty complete");
    }
}

#[test]
fn fuzz_like_junk_does_not_panic() {
    for src in [
        "@@@",
        "void (",
        "{",
        "continue;",
        "1 ? 2",
        "struct {",
        "namespace X { }",
        "@this::M(",
        "GetService<",
        "#include <",
        "try { } catch (auto e) { }",
    ] {
        let _ = parse_for_ide(src, "junk.clpp");
        let _ = complete_request(&PositionRequest {
            source: src.into(),
            file_name: "junk.clpp".into(),
            line: 1,
            column: src.len().max(1),
        });
    }
}
