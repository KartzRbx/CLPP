//! Builtin registry (Luau `Builtins.cpp` analogue). One table feeds emit, manifest, and IDE.

#[derive(Debug, Clone, Copy)]
pub struct Builtin {
    pub clpp: &'static str,
    pub luau: &'static str,
    pub kind: &'static str,
    pub min_arity: usize,
    pub max_arity: usize,
    pub detail: &'static str,
}

pub const BUILTINS: &[Builtin] = &[
    Builtin {
        clpp: "post",
        luau: "print",
        kind: "Function",
        min_arity: 0,
        max_arity: usize::MAX,
        detail: "Write a line to output (Luau print).",
    },
    Builtin {
        clpp: "warn",
        luau: "warn",
        kind: "Function",
        min_arity: 0,
        max_arity: usize::MAX,
        detail: "Write a warning.",
    },
    Builtin {
        clpp: "report",
        luau: "error",
        kind: "Function",
        min_arity: 1,
        max_arity: usize::MAX,
        detail: "Stop the script with an error (Luau error).",
    },
    Builtin {
        clpp: "to_string",
        luau: "tostring",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Convert a value to text.",
    },
    Builtin {
        clpp: "to_number",
        luau: "tonumber",
        kind: "Function",
        min_arity: 1,
        max_arity: 2,
        detail: "Parse a number from text.",
    },
    Builtin {
        clpp: "to_bool",
        luau: "not not",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Convert a value to bool.",
    },
    Builtin {
        clpp: "string_concat",
        luau: "..",
        kind: "Function",
        min_arity: 0,
        max_arity: usize::MAX,
        detail: "Join strings (Luau ..).",
    },
    Builtin {
        clpp: "GetService",
        luau: "game:GetService",
        kind: "Function",
        min_arity: 0,
        max_arity: 0,
        detail: "GetService<T>() — Roblox service.",
    },
    Builtin {
        clpp: "print",
        luau: "print",
        kind: "Function",
        min_arity: 0,
        max_arity: usize::MAX,
        detail: "Luau print. Prefer post.",
    },
    Builtin {
        clpp: "error",
        luau: "error",
        kind: "Function",
        min_arity: 1,
        max_arity: usize::MAX,
        detail: "Luau error. Prefer report.",
    },
    Builtin {
        clpp: "tostring",
        luau: "tostring",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Luau tostring. Prefer to_string.",
    },
    Builtin {
        clpp: "tonumber",
        luau: "tonumber",
        kind: "Function",
        min_arity: 1,
        max_arity: 2,
        detail: "Luau tonumber. Prefer to_number.",
    },
];

pub const KEYWORDS: &[&str] = &[
    "void", "int", "float", "double", "bool", "string", "auto", "func", "const", "observable",
    "signal", "struct", "return", "if", "else", "while", "for", "in", "switch", "case", "default",
    "break", "guard", "match", "spawn", "parallel", "async", "await", "new", "null", "true", "false",
    "this",
];

pub const INSTANCE_PROPS: &[&str] = &[
    "Name", "ClassName", "Parent", "DisplayName", "UserId", "AccountAge", "Character", "Value",
    "Text", "Enabled", "Visible", "Size", "Position", "CFrame", "Color", "Transparency",
];

pub const INSTANCE_METHODS: &[&str] = &[
    "FindFirstChild", "FindFirstChildOfClass", "FindFirstChildWhichIsA", "WaitForChild",
    "GetChildren", "GetDescendants", "GetPlayers", "GetPlayerFromCharacter", "GetService", "IsA",
    "Clone", "Destroy", "ClearAllChildren", "GetAttribute", "SetAttribute", "GetPropertyChangedSignal",
    "Kick", "LoadCharacter", "Connect", "Once", "Wait", "Fire", "OnChange", "Add", "Cleanup",
];

pub fn find(name: &str) -> Option<&'static Builtin> {
    BUILTINS.iter().find(|b| b.clpp == name)
}

/// Map a CL++ call name to the Luau identifier (or keep the name).
pub fn map_name(name: &str) -> &str {
    find(name)
        .map(|b| b.luau)
        .filter(|luau| *luau != ".." && *luau != "not not" && !luau.contains(':'))
        .unwrap_or(name)
}

pub fn names() -> Vec<&'static str> {
    BUILTINS.iter().map(|b| b.clpp).collect()
}

pub fn is_variadic(name: &str) -> bool {
    find(name).is_some_and(|b| b.max_arity == usize::MAX)
}

pub fn arity_ok(name: &str, count: usize) -> bool {
    match find(name) {
        Some(b) => count >= b.min_arity && count <= b.max_arity,
        None => true,
    }
}
