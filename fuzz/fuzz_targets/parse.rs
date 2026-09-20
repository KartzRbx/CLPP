#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    if let Ok(src) = std::str::from_utf8(data) {
        if src.len() > 4096 {
            return;
        }
        let _ = clpp::parser::parse_for_ide(src, "fuzz.clpp");
    }
});
