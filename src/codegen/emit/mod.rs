//! Luau codegen split: `engine` owns `Emitter`; `helpers` re-exports the builtin registry.

mod engine;
mod helpers;

pub use engine::emit;
pub use helpers::map_name;
