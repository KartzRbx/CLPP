//! Optional host platforms. The CL++ compiler core does not require Roblox.
//!
//! Cluaupp (and [`crate::session::Session::with_roblox_platform`]) load the Roblox API here.
//! `Session::language()` skips this module.
//!
//! ## Cluaupp artifact hooks
//! After compile, hosts should read [`crate::support::CompileArtifact`]:
//! - `nativeHints` → selective `@native`
//! - `layoutHints` → SoA / buffer decisions
//! - `sourceMap` → Studio debugging
//! - `optimized` → whether RFC 0011 opts ran

pub use crate::names::{is_instance_type, INSTANCE_TYPES};
pub use crate::roblox::{is_service, load_prelude};

/// Stable capability tags Cluaupp may attach to functions (metadata only in clpp).
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HostCapability {
    NativeCandidate,
    BufferCandidate,
    SoaCandidate,
    ServerOnly,
    ClientOnly,
}

/// Map layout/native hint strings from the optimizer into host capabilities.
pub fn capabilities_from_hints(native: &[String], layout: &[String]) -> Vec<(String, HostCapability)> {
    let mut out = Vec::new();
    for n in native {
        out.push((n.clone(), HostCapability::NativeCandidate));
    }
    for h in layout {
        if let Some(rest) = h.strip_prefix("BufferCandidate:") {
            out.push((rest.to_string(), HostCapability::BufferCandidate));
        } else if let Some(rest) = h.strip_prefix("BufferSpecialize:") {
            out.push((rest.to_string(), HostCapability::BufferCandidate));
        } else if let Some(rest) = h.strip_prefix("SoA:") {
            out.push((rest.to_string(), HostCapability::SoaCandidate));
        } else if let Some(rest) = h.strip_prefix("SoACandidate:") {
            out.push((rest.to_string(), HostCapability::SoaCandidate));
        } else if let Some(rest) = h.strip_prefix("DenseNumeric:") {
            out.push((rest.to_string(), HostCapability::BufferCandidate));
        }
    }
    out
}
