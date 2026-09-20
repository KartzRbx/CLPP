# cargo-fuzz

Nightly + [cargo-fuzz](https://github.com/rust-fuzz/cargo-fuzz):

```bash
cargo install cargo-fuzz
cargo fuzz run parse
cargo fuzz run complete
```

CI on this repo runs `cargo test` junk recovery (`tests/v07.rs::fuzz_like_junk_does_not_panic`) so Windows/macOS stay green without libFuzzer.
