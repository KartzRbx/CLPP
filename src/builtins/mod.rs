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
    Builtin {
        clpp: "div",
        luau: "//",
        kind: "Function",
        min_arity: 2,
        max_arity: 2,
        detail: "Integer division (Luau //).",
    },
    Builtin {
        clpp: "pow",
        luau: "^",
        kind: "Function",
        min_arity: 2,
        max_arity: 2,
        detail: "Power (Luau ^).",
    },
    Builtin {
        clpp: "size",
        luau: "#",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Length (Luau #).",
    },
    Builtin {
        clpp: "band",
        luau: "bit32.band",
        kind: "Function",
        min_arity: 2,
        max_arity: usize::MAX,
        detail: "Bitwise and.",
    },
    Builtin {
        clpp: "bor",
        luau: "bit32.bor",
        kind: "Function",
        min_arity: 2,
        max_arity: usize::MAX,
        detail: "Bitwise or.",
    },
    Builtin {
        clpp: "bxor",
        luau: "bit32.bxor",
        kind: "Function",
        min_arity: 2,
        max_arity: usize::MAX,
        detail: "Bitwise xor.",
    },
    Builtin {
        clpp: "bnot",
        luau: "bit32.bnot",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Bitwise not.",
    },
    Builtin {
        clpp: "lshift",
        luau: "bit32.lshift",
        kind: "Function",
        min_arity: 2,
        max_arity: 2,
        detail: "Bitwise left shift.",
    },
    Builtin {
        clpp: "rshift",
        luau: "bit32.rshift",
        kind: "Function",
        min_arity: 2,
        max_arity: 2,
        detail: "Bitwise right shift.",
    },
    Builtin {
        clpp: "delay",
        luau: "task.delay",
        kind: "Function",
        min_arity: 1,
        max_arity: 2,
        detail: "Run a callback after t seconds.",
    },
    Builtin {
        clpp: "defer",
        luau: "task.defer",
        kind: "Function",
        min_arity: 0,
        max_arity: 1,
        detail: "Run a callback on the next resumption.",
    },
    Builtin {
        clpp: "cancel",
        luau: "task.cancel",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Cancel a delay/defer thread.",
    },
    Builtin {
        clpp: "assert",
        luau: "assert",
        kind: "Function",
        min_arity: 1,
        max_arity: 2,
        detail: "Runtime assert; narrows truthy values.",
    },
    Builtin {
        clpp: "debug_assert",
        luau: "assert",
        kind: "Function",
        min_arity: 1,
        max_arity: 2,
        detail: "Stripped in RELEASE; assert in DEBUG.",
    },
    Builtin {
        clpp: "static_assert",
        luau: "assert",
        kind: "Function",
        min_arity: 1,
        max_arity: 2,
        detail: "Compile-time constant assertion.",
    },
    Builtin {
        clpp: "unreachable",
        luau: "error",
        kind: "Function",
        min_arity: 0,
        max_arity: 1,
        detail: "Marks a path that must not run.",
    },
    Builtin {
        clpp: "Some",
        luau: "Some",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Option present (RFC 0012). Emits the value.",
    },
    Builtin {
        clpp: "Ok",
        luau: "Ok",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Result success (RFC 0012). Emits { ok = v }.",
    },
    Builtin {
        clpp: "Err",
        luau: "Err",
        kind: "Function",
        min_arity: 1,
        max_arity: 1,
        detail: "Result failure (RFC 0012). Emits { err = e }.",
    },
];

pub const KEYWORDS: &[&str] = &[
    "void", "int", "float", "double", "bool", "string", "auto", "func", "const", "observable",
    "signal", "struct", "interface", "type", "return", "if", "else", "while", "for", "in", "switch", "case", "default",
    "break", "continue", "guard", "match", "spawn", "parallel", "async", "await", "new", "null",
    "true", "false", "this", "enum", "using", "try", "catch", "do", "delay", "defer", "comptime", "template",
    "typename", "override", "static", "private", "public", "class",
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
        .filter(|luau| *luau != ".." && *luau != "not not" && *luau != "//" && *luau != "^" && *luau != "#" && !luau.contains(':'))
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
