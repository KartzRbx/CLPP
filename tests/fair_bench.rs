use clpp::compile::{compile_artifact_source, compile_source_opts};
use std::path::Path;

mod fixtures {
    include!("../benches/fixtures.rs");
}

#[test]
fn fair_suite_fixtures_compile() {
    for (name, src) in [
        ("numeric", fixtures::NUMERIC),
        ("data", fixtures::DATA),
        ("realistic", fixtures::REALISTIC),
    ] {
        let art = compile_artifact_source(src, Path::new(&format!("{name}.clpp")), None)
            .unwrap_or_else(|e| panic!("{name}: {e:#}"));
        assert!(art.ok, "{name}: {:?}", art.error);
        assert!(art.optimized, "{name} should opt by default");
    }
}

#[test]
fn opts_off_baseline_keeps_const_mul() {
    let src = r#"
const float BASE = 120.0;
const float MULT = 1.5;
float Damage() { return BASE * MULT; }
"#;
    let off = compile_source_opts(src, Path::new("off.clpp"), false).expect("off");
    let on = compile_source_opts(src, Path::new("on.clpp"), true).expect("on");
    assert!(
        on.contains("180") || on.contains("180.0"),
        "opts on should fold: {on}"
    );
    // Baseline D: no fold of the product (identifiers or raw consts may remain).
    assert!(
        !off.contains("180") && !off.contains("180.0"),
        "opts off should not fold to 180: {off}"
    );
}

#[test]
fn layout_hints_for_dense_arrays() {
    let art = compile_artifact_source(
        fixtures::DATA,
        Path::new("data.clpp"),
        None,
    )
    .expect("data");
    assert!(art.ok, "{:?}", art.error);
    assert!(
        art.layout_hints.iter().any(|h| h.starts_with("DenseNumeric:") || h.starts_with("BufferCandidate:")),
        "expected layout hints, got {:?}",
        art.layout_hints
    );
}

#[test]
fn option_result_emit() {
    let luau = clpp::compile::compile_source(
        r#"
Option<int> A() { return Some(1); }
Option<int> B() { return None; }
Result<int, string> C() { return Ok(2); }
Result<int, string> D() { return Err("x"); }
"#,
        Path::new("or.clpp"),
    )
    .expect("compile");
    assert!(luau.contains("return 1") || luau.contains("= 1"), "{luau}");
    assert!(luau.contains("nil"), "{luau}");
    assert!(luau.contains("ok ="), "{luau}");
    assert!(luau.contains("err ="), "{luau}");
}
