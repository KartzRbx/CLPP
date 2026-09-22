//! Real compile-throughput + type-check differential benches.
//!
//! ```text
//! cargo test --offline --test bench_compile -- --nocapture
//! ```
//! Writes `docs/benchmarks/results.json` for the site page.

use clpp::compile::{compile_artifact_source, compile_source};
use clpp::session::Session;
use serde::Serialize;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

const ITERS: u32 = 25;
const WARMUP: u32 = 3;

#[derive(Serialize)]
struct BenchRow {
    name: &'static str,
    iters: u32,
    median_ms: f64,
    p95_ms: f64,
    loc: usize,
    loc_per_sec: f64,
}

#[derive(Serialize)]
struct CatchRow {
    name: &'static str,
    code: &'static str,
    caught: bool,
}

#[derive(Serialize)]
struct Report {
    generated_at: String,
    rustc_opt: &'static str,
    rows: Vec<BenchRow>,
    catches: Vec<CatchRow>,
    session_repeat_ms: f64,
}

fn median(sorted: &[Duration]) -> f64 {
    let n = sorted.len();
    if n == 0 {
        return 0.0;
    }
    if n % 2 == 1 {
        sorted[n / 2].as_secs_f64() * 1000.0
    } else {
        let a = sorted[n / 2 - 1].as_secs_f64();
        let b = sorted[n / 2].as_secs_f64();
        ((a + b) / 2.0) * 1000.0
    }
}

fn p95(sorted: &[Duration]) -> f64 {
    if sorted.is_empty() {
        return 0.0;
    }
    let idx = ((sorted.len() as f64) * 0.95).ceil() as usize - 1;
    sorted[idx.min(sorted.len() - 1)].as_secs_f64() * 1000.0
}

fn measure(src: &str, path: &str) -> (Vec<Duration>, usize) {
    let loc = src.lines().filter(|l| !l.trim().is_empty()).count();
    for _ in 0..WARMUP {
        let _ = compile_source(src, Path::new(path));
    }
    let mut samples = Vec::with_capacity(ITERS as usize);
    for _ in 0..ITERS {
        let t0 = Instant::now();
        let _ = compile_artifact_source(src, Path::new(path), None);
        samples.push(t0.elapsed());
    }
    samples.sort();
    (samples, loc)
}

fn row(name: &'static str, src: &str, path: &str) -> BenchRow {
    let (samples, loc) = measure(src, path);
    let med = median(&samples);
    let loc_per_sec = if med > 0.0 {
        (loc as f64) / (med / 1000.0)
    } else {
        0.0
    };
    BenchRow {
        name,
        iters: ITERS,
        median_ms: med,
        p95_ms: p95(&samples),
        loc,
        loc_per_sec,
    }
}

fn has_code(src: &str, code: &str) -> bool {
    let art = compile_artifact_source(src, Path::new("catch.clpp"), None).expect("artifact");
    art.diagnostics
        .iter()
        .any(|d| d.code.as_deref() == Some(code) || d.message.contains(code))
}

#[test]
fn compile_throughput_and_type_catches() {
    let features = include_str!("../examples/syntax/features.clp");
    let config = include_str!("../examples/shared/config.clp");
    let player_h = include_str!("../examples/shared/PlayerData.clh");
    let use_pd = include_str!("../examples/shared/use_player_data.clp");

    let generic_ok = r#"
interface Drawable { void render(); };
struct Circle { void render(); int r; };
template <typename T : Drawable>
void draw(T item) { item.render(); }
void F(Circle c) { draw(c); }
"#;

    let mut rows = vec![
        row("syntax/features.clp", features, "features.clp"),
        row("shared/config.clp", config, "config.clp"),
        row("checked_generics_ok", generic_ok, "gen.clpp"),
        row(
            "synthetic_200_stmts",
            &synthetic_program(200),
            "synth.clpp",
        ),
    ];
    let _ = (player_h, use_pd);

    // Session warm path: same bytes should hash-skip after first check.
    let mut session = Session::new();
    let warm_src = features;
    let _ = session.check_source(warm_src, "warm.clpp");
    let mut warm_samples = Vec::new();
    for _ in 0..ITERS {
        let t0 = Instant::now();
        let _ = session.check_source(warm_src, "warm.clpp");
        warm_samples.push(t0.elapsed());
    }
    warm_samples.sort();
    let session_repeat_ms = median(&warm_samples);

    let catches = vec![
        CatchRow {
            name: "optional → plain (CLPP0201)",
            code: "CLPP0201",
            caught: {
                let art = compile_artifact_source(
                    "void F(optional<Player> p) { Player x = p; }\n",
                    Path::new("c.clpp"),
                    None,
                )
                .unwrap();
                art.diagnostics
                    .iter()
                    .any(|d| d.code.as_deref() == Some("CLPP0201"))
            },
        },
        CatchRow {
            name: "private member (CLPP0401)",
            code: "CLPP0401",
            caught: has_code(
                r#"
struct Box { private: int secret; public: void Show(); };
void Box::Show() { @secret = 1; }
void F(Box b) { b.secret = 2; }
"#,
                "CLPP0401",
            ),
        },
        CatchRow {
            name: "generic bound (CLPP0901)",
            code: "CLPP0901",
            caught: has_code(
                r#"
interface Drawable { void render(); };
struct Plain { int n; };
template <typename T : Drawable>
void draw(T item) { item.render(); }
void F(Plain p) { draw(p); }
"#,
                "CLPP0901",
            ),
        },
        CatchRow {
            name: "static_assert (CLPP0701)",
            code: "CLPP0701",
            caught: has_code("void F() { static_assert(false); }\n", "CLPP0701"),
        },
        CatchRow {
            name: "bare method (CLPP0101)",
            code: "CLPP0101",
            caught: has_code(
                r#"
struct Actor { void B(int x); void Tick(); };
void Actor::B(int x) { post(x); }
void Actor::Tick() { B(1); }
"#,
                "CLPP0101",
            ),
        },
    ];

    for c in &catches {
        assert!(c.caught, "expected catch {}", c.name);
    }
    for r in &rows {
        assert!(r.median_ms > 0.0, "{}", r.name);
        // Sanity: even synthetic 1k should finish under a few seconds median in debug.
        assert!(
            r.median_ms < 5_000.0,
            "{} too slow: {} ms",
            r.name,
            r.median_ms
        );
    }

    let report = Report {
        generated_at: chrono_like(),
        rustc_opt: if cfg!(debug_assertions) {
            "debug"
        } else {
            "release"
        },
        rows: rows.drain(..).collect(),
        catches,
        session_repeat_ms,
    };

    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let out = root.join("docs/benchmarks/results.json");
    if let Some(parent) = out.parent() {
        let _ = std::fs::create_dir_all(parent);
    }
    let json = serde_json::to_string_pretty(&report).expect("json");
    std::fs::write(&out, json).expect("write results.json");
    println!(
        "wrote {} (session repeat median {:.3} ms)",
        out.display(),
        report.session_repeat_ms
    );
    for r in &report.rows {
        println!(
            "  {:>28}  med={:.3} ms  p95={:.3} ms  ~{:.0} loc/s",
            r.name, r.median_ms, r.p95_ms, r.loc_per_sec
        );
    }
}

fn synthetic_program(n: usize) -> String {
    let mut s = String::from("void F() {\n");
    for i in 0..n {
        s.push_str(&format!("    int x{i} = {i};\n"));
        s.push_str(&format!("    post(x{i});\n"));
    }
    s.push_str("}\n");
    s
}

fn chrono_like() -> String {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| format!("unix_ms={}", d.as_millis()))
        .unwrap_or_else(|_| "unknown".into())
}
