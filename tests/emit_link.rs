use clpp::compile::compile_artifact_source;
use std::path::Path;

fn compile(src: &str, path: &Path) -> clpp::support::CompileArtifact {
    compile_artifact_source(src, path, None).expect("artifact")
}

#[test]
fn package_link_is_not_a_script_path() {
    let art = compile("link @clpp.roblox;\nvoid init() {}\n", Path::new("main.clp"));
    assert!(!art.ok, "package links are not resolved yet");
    assert!(
        art.diagnostics.iter().any(|d| {
            d.code.as_deref() == Some("CLPP0802")
                && d.message.contains("package link `@clpp.roblox`")
        }),
        "{:?}",
        art.diagnostics
    );
    assert!(
        !art.luau.contains("script.Parent") && !art.luau.contains("require("),
        "got {}",
        art.luau
    );
    let game = compile(
        "link @game.ReplicatedStorage.Modules.Combat as CombatModule;\nvoid init() {}\n",
        Path::new("main.clp"),
    );
    assert!(!game.ok);
    assert!(
        !game.luau.contains("script.Parent") && !game.luau.contains("@clpp") && !game.luau.contains("require("),
        "got {}",
        game.luau
    );
}

#[test]
fn dotted_stem_is_not_a_const_name() {
    let dir = std::env::temp_dir().join(format!("clpp-stem-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    std::fs::write(dir.join("a.b.clp"), "void init() {}\n").unwrap();
    let consumer = dir.join("Main.clpp");
    let src = "link \"./a.b.clp\";\nvoid init() {}\n";
    let art = compile(src, &consumer);
    let _ = std::fs::remove_dir_all(&dir);
    assert!(!art.ok, "{:?}", art.diagnostics);
    assert!(
        art.diagnostics
            .iter()
            .any(|d| d.message.contains("`a.b` is not a binding name")),
        "{:?}",
        art.diagnostics
    );
    assert!(!art.luau.contains("const a.b"), "got {}", art.luau);
    assert!(!art.luau.contains("require("), "got {}", art.luau);
}
