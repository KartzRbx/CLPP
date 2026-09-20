#![no_main]
use libfuzzer_sys::fuzz_target;
use clpp::analysis::{complete_request, PositionRequest};

fuzz_target!(|data: &[u8]| {
    if let Ok(src) = std::str::from_utf8(data) {
        if src.len() > 2048 {
            return;
        }
        let _ = complete_request(&PositionRequest {
            source: src.to_string(),
            file_name: "fuzz.clpp".into(),
            line: 1,
            column: src.len().max(1),
        });
    }
});
