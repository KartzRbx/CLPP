use clpp::compile_request;
use clpp::support::{language_manifest, CompileRequest};
use std::path::PathBuf;

fn example(rel: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("examples").join(rel)
}

#[test]
fn api_compile_hello_json_shape() {
    let source = std::fs::read_to_string(example("hello/hello.server.clpp")).unwrap();
    let art = compile_request(&CompileRequest {
        source,
        file_name: "hello.server.clpp".into(),
        strict: None,
    })
    .expect("compile");
    assert!(art.ok);
    assert_eq!(art.rojo_class, "Script");
    assert_eq!(art.script_kind.as_deref(), Some("server"));
    assert!(art.output_hint.ends_with("hello.server.luau"));
    assert!(art.luau.contains("game:GetService(\"Players\")"));
}

#[test]
fn api_manifest_lists_language() {
    let manifest = language_manifest();
    assert_eq!(manifest.id, "clpp");
    assert!(manifest.extensions.contains(&".clpp"));
    assert!(manifest.operators.iter().any(|op| op.clpp == ".:"));
}
