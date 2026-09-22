//! Optional host platforms. The CL++ compiler core does not require Roblox.
//!
//! Cluaupp (and [`crate::session::Session::with_roblox_platform`]) load the Roblox API here.
//! `Session::language()` skips this module.

pub use crate::roblox::{is_service, load_prelude};
pub use crate::names::{is_instance_type, INSTANCE_TYPES};
