//! `clpp_cli build` and `clpp_cli lsp`.
//!
//! Not named `clpp`: the root package still owns that binary until Pest is removed.

fn main() {
    let code = clpp_cli::run(std::env::args().skip(1));
    std::process::exit(code);
}
