use clpp::compile_request;
use clpp::support::{language_manifest, CompileRequest};

#[test]
fn api_compile_server_json_shape() {
    let source = r#"
struct Hello {
    void Greet();
};
void Hello::Greet() {
    post("hi");
}
void init() {
    Hello h;
    h.Greet();
}
"#;
    let art = compile_request(&CompileRequest {
        source: source.into(),
        file_name: "hello.server.clpp".into(),
        strict: None,
        optimize: None,
    })
    .expect("compile");
    assert!(art.ok, "{:?}", art.diagnostics);
    assert_eq!(art.rojo_class, "Script");
    assert_eq!(art.script_kind.as_deref(), Some("server"));
    assert!(art.output_hint.ends_with("hello.server.luau"));
    assert!(art.luau.contains("function Hello:Greet"));
}

#[test]
fn api_manifest_lists_language() {
    let manifest = language_manifest();
    assert_eq!(manifest.id, "clpp");
    assert!(manifest.extensions.contains(&".clpp"));
    assert!(manifest.operators.iter().any(|op| op.clpp == ".:"));
}
