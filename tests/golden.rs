use clpp::support::CompileRequest;
use clpp::{compile_artifact_source_ex, compile_request};
use std::{
    fs,
    io::{BufRead, BufReader, Write},
    path::{Path, PathBuf},
    process::{Command, Stdio},
};

#[test]
fn golden_luau_is_exact() {
    let cases = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/cases");
    let mut files: Vec<_> = fs::read_dir(&cases)
        .unwrap()
        .map(|e| e.unwrap().path())
        .filter(|p| p.extension().is_some_and(|e| e == "clpp"))
        .collect();
    files.sort();
    for path in files {
        let name = path.file_name().unwrap().to_str().unwrap();
        let source = fs::read_to_string(&path).unwrap();
        let artifact = compile_artifact_source_ex(
            &source,
            &PathBuf::from(format!("tests/golden/cases/{name}")),
            Some(true),
            Some(false),
        )
        .unwrap();
        assert!(artifact.ok, "{name}: {:?}", artifact.diagnostics);
        let expected = fs::read_to_string(path.with_extension("luau"))
            .unwrap()
            .replace("\r\n", "\n");
        assert_eq!(artifact.luau, expected, "{name}");
    }
}

#[test]
fn lib_root_is_emitted_and_invalid_paths_are_diagnosed() {
    let request = |root: &str| CompileRequest {
        source: "link @clpp.libs.roster as Roster; void init() { auto roster = new Roster(); }"
            .into(),
        file_name: "a.server.clpp".into(),
        strict: Some(true),
        optimize: Some(false),
        lib_root: Some(root.into()),
    };
    let artifact = compile_request(&request("ReplicatedStorage.CluauppLibs")).unwrap();
    assert!(artifact.ok, "{:?}", artifact.diagnostics);
    assert!(artifact
        .luau
        .contains("require(ReplicatedStorage.CluauppLibs.Roster)"));
    assert!(artifact.luau.contains("Roster.new()"));
    assert!(!artifact.luau.contains("Instance.new(\"Roster\")"));
    assert!(!artifact.luau.contains("ClppLibs"));
    let bad = compile_request(&request("x); print(1)--")).unwrap();
    assert!(!bad.ok);
    assert_eq!(bad.diagnostics[0].code.as_deref(), Some("CLPP0004"));
}

#[test]
fn source_map_has_real_declaration_columns() {
    let source = "void init() {\n    int coins = 3;\n    post(coins);\n}\n";
    let artifact = compile_artifact_source_ex(
        source,
        Path::new("span.server.clpp"),
        Some(true),
        Some(false),
    )
    .unwrap();
    assert!(artifact.ok);
    let entry = artifact
        .source_map
        .iter()
        .find(|e| {
            artifact
                .luau
                .lines()
                .nth(e.luau_line - 1)
                .unwrap()
                .contains("local coins")
        })
        .unwrap();
    assert_eq!((entry.clpp_line, entry.clpp_column), (2, 5));
    assert_eq!(entry.luau_column, 2); // one tab; columns count characters
    assert_eq!(entry.file, "span.server.clpp");
    assert!(entry.span.end_col > entry.span.start_col);
}

#[test]
fn serve_flushes_each_response_and_recovers_after_bad_json() {
    let mut process = Command::new(env!("CARGO_BIN_EXE_clpp"))
        .args(["api", "serve"])
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();
    let mut input = process.stdin.take().unwrap();
    let request = serde_json::json!({"source":"void init() {}", "fileName":"a.server.clpp"});
    let stdout = process.stdout.take().unwrap();
    let (send, receive) = std::sync::mpsc::channel();
    let reader = std::thread::spawn(move || {
        for line in BufReader::new(stdout).lines() {
            send.send(line.unwrap()).unwrap();
        }
    });
    let mut responses: Vec<serde_json::Value> = Vec::new();
    for line in [request.to_string(), "{broken".into(), request.to_string()] {
        writeln!(input, "{line}").unwrap();
        input.flush().unwrap();
        match receive.recv_timeout(std::time::Duration::from_secs(10)) {
            Ok(line) => responses.push(serde_json::from_str(&line).unwrap()),
            Err(err) => {
                process.kill().unwrap();
                panic!("response was not flushed before EOF: {err}");
            }
        }
    }
    drop(input);
    assert!(process.wait().unwrap().success());
    reader.join().unwrap();
    assert_eq!(responses.len(), 3);
    assert_eq!(responses[0]["ok"], true);
    assert_eq!(responses[1]["ok"], false);
    assert_eq!(responses[1]["diagnostics"][0]["code"], "CLPP0005");
    assert_eq!(responses[2]["ok"], true);
    assert_eq!(
        responses[2]["contractVersion"],
        clpp::support::CONTRACT_VERSION
    );
}

#[test]
fn link_cycles_are_reported_without_recursion() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("target")
        .join(format!("link-cycle-{}", std::process::id()));
    fs::create_dir_all(&root).unwrap();
    fs::write(root.join("A.clp"), "link \"./B.clp\" as B; void A() {}\n").unwrap();
    fs::write(root.join("B.clp"), "link \"./A.clp\" as A; void B() {}\n").unwrap();
    let source = fs::read_to_string(root.join("A.clp")).unwrap();
    let artifact =
        compile_artifact_source_ex(&source, &root.join("A.clp"), Some(true), Some(false)).unwrap();
    assert!(!artifact.ok);
    assert!(artifact
        .diagnostics
        .iter()
        .any(|d| d.code.as_deref() == Some("CLPP1001")));
}

#[test]
fn all_failed_compilations_have_stable_diagnostics() {
    for source in [
        "void init( {",
        "void init() { int n = \"wrong\"; }",
        "#include <clpp/roblox.clh>",
    ] {
        let artifact =
            compile_artifact_source_ex(source, Path::new("bad.clpp"), None, Some(false)).unwrap();
        assert!(!artifact.ok);
        assert!(!artifact.diagnostics.is_empty());
        for d in artifact.diagnostics {
            assert!(d.code.is_some());
            assert!(d.help.is_some());
            assert!(d.span.start_line >= 1 && d.span.start_col >= 1);
        }
    }
}

#[test]
fn linked_source_interfaces_are_checked_before_consumers() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("target")
        .join(format!("link-interface-{}", std::process::id()));
    fs::create_dir_all(&root).unwrap();
    fs::write(
        root.join("Math.clp"),
        "int twice(int n) { return n * 2; }\n",
    )
    .unwrap();
    for (argument, ok) in [("2", true), ("\"wrong\"", false)] {
        let source = format!(
            "link \"./Math.clp\" as Math; void init() {{ int n = Math.twice({argument}); }}"
        );
        let artifact = compile_artifact_source_ex(
            &source,
            &root.join("main.server.clpp"),
            Some(true),
            Some(false),
        )
        .unwrap();
        assert_eq!(artifact.ok, ok, "{:?}", artifact.diagnostics);
        let (file, _) =
            clpp::session::analyze(&source, root.join("main.server.clpp").to_str().unwrap());
        assert_eq!(
            file.diagnostics.iter().all(|d| d.severity == "warning"),
            ok,
            "{:?}",
            file.diagnostics
        );
    }
}

#[test]
fn propagation_without_a_result_scope_is_a_diagnostic() {
    for source in [
        "Result<int,string> value = Ok(1); int n = value?;",
        "void init() { int n = Ok(1)?; }",
    ] {
        let artifact =
            compile_artifact_source_ex(source, Path::new("bad.clpp"), None, Some(false)).unwrap();
        assert!(!artifact.ok);
        assert!(artifact
            .diagnostics
            .iter()
            .any(|d| d.code.as_deref() == Some("CLPP1103")));
    }
}
