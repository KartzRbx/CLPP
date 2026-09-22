use clpp::compile::{compile_artifact_source, compile_source};
use std::path::Path;

#[test]
fn folds_const_arithmetic() {
    let luau = compile_source(
        r#"
const float BASE_DAMAGE = 120;
const float CRIT_MULTIPLIER = 1.5;
float Damage() {
    return BASE_DAMAGE * CRIT_MULTIPLIER;
}
"#,
        Path::new("fold.clpp"),
    )
    .expect("compile");
    assert!(
        luau.contains("180") || luau.contains("180.0"),
        "expected folded 180, got:\n{luau}"
    );
}

#[test]
fn eliminates_if_false() {
    let luau = compile_source(
        r#"
const bool DEBUG = false;
void F() {
    if (DEBUG) {
        post("secret");
    }
    post("ok");
}
"#,
        Path::new("dce.clpp"),
    )
    .expect("compile");
    assert!(!luau.contains("secret"), "dead branch survived:\n{luau}");
    assert!(luau.contains("ok"), "{luau}");
}

#[test]
fn folds_local_const_in_function() {
    let luau = compile_source(
        r#"
void F() {
    const int a = 10;
    const int b = 20;
    post(a + b);
}
"#,
        Path::new("local.clpp"),
    )
    .expect("compile");
    assert!(luau.contains("30"), "expected 30, got:\n{luau}");
}

#[test]
fn inlines_small_pure_function() {
    let luau = compile_source(
        r#"
float Square(float x) {
    return x * x;
}
float F(float v) {
    return Square(v);
}
"#,
        Path::new("inline.clpp"),
    )
    .expect("compile");
    assert!(
        luau.contains("v * v") || luau.contains("(v) * (v)"),
        "expected inlined multiply, got:\n{luau}"
    );
    // Call site should not retain Square(v) as a call.
    assert!(
        !luau.contains("Square(v)"),
        "call survived:\n{luau}"
    );
}

#[test]
fn scalar_replaces_init_list_fields() {
    let luau = compile_source(
        r#"
struct Point { float X; float Y; };
float Len() {
    Point p = { .X = 3.0, .Y = 4.0 };
    return p.X * p.X + p.Y * p.Y;
}
"#,
        Path::new("scalar.clpp"),
    )
    .expect("compile");
    let folded = luau.contains("25") || luau.contains("25.0");
    let scalars = luau.contains("p__X") || luau.contains("p__Y");
    assert!(
        folded || scalars,
        "expected scalar replace or fold, got:\n{luau}"
    );
    assert!(
        !luau.contains("p.X") && !luau.contains("{ X ="),
        "object form survived:\n{luau}"
    );
}

#[test]
fn monomorphizes_explicit_type_args() {
    let luau = compile_source(
        r#"
template <typename T>
T Id(T x) {
    return x;
}
int F(int n) {
    return Id<int>(n);
}
"#,
        Path::new("mono.clpp"),
    )
    .expect("compile");
    assert!(
        luau.contains("Id__int") || !luau.contains("Id<"),
        "expected specialized symbol, got:\n{luau}"
    );
}

#[test]
fn native_hints_for_numeric_loop() {
    let art = compile_artifact_source(
        r#"
float Hot(float acc, int n) {
    for (int i = 0; i < n; i++) {
        acc = acc + i * 1.5;
    }
    return acc;
}
void UI() {
    post("hi");
}
"#,
        Path::new("native.clpp"),
        None,
    )
    .expect("artifact");
    assert!(art.ok, "{:?}", art.error);
    assert!(
        art.native_hints.iter().any(|h| h == "Hot"),
        "expected Hot hint, got {:?}",
        art.native_hints
    );
    assert!(
        !art.native_hints.iter().any(|h| h == "UI"),
        "UI should not be native-hinted: {:?}",
        art.native_hints
    );
}
