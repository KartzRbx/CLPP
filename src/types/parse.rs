//! Parse a CL++ type string (`optional<Instance>`, `A | B`, `array<T>`) into a `TypeId`.

use super::{TypeDatabase, TypeId, TypeKind};

pub fn parse_type_str(db: &mut TypeDatabase, raw: &str) -> TypeId {
    let cleaned = raw
        .trim()
        .trim_end_matches('*')
        .trim()
        .trim_start_matches("const ")
        .replace("Enum::", "Enum.");
    if cleaned.is_empty() {
        return db.unknown;
    }
    let mut p = Parser {
        src: cleaned.chars().collect(),
        i: 0,
        db,
    };
    let id = p.parse_union();
    p.skip();
    if p.i < p.src.len() {
        // Best-effort: leftover characters still produced a type.
    }
    id
}

struct Parser<'a> {
    src: Vec<char>,
    i: usize,
    db: &'a mut TypeDatabase,
}

impl Parser<'_> {
    fn skip(&mut self) {
        while self.i < self.src.len() && self.src[self.i].is_whitespace() {
            self.i += 1;
        }
    }

    fn peek(&mut self) -> Option<char> {
        self.skip();
        self.src.get(self.i).copied()
    }

    fn bump(&mut self) -> Option<char> {
        self.skip();
        if self.i < self.src.len() {
            let c = self.src[self.i];
            self.i += 1;
            Some(c)
        } else {
            None
        }
    }

    fn eat(&mut self, c: char) -> bool {
        if self.peek() == Some(c) {
            self.i += 1;
            true
        } else {
            false
        }
    }

    fn parse_union(&mut self) -> TypeId {
        let mut parts = vec![self.parse_intersection()];
        while self.eat('|') {
            parts.push(self.parse_intersection());
        }
        self.db.union_of(parts)
    }

    fn parse_intersection(&mut self) -> TypeId {
        let mut parts = vec![self.parse_app()];
        while self.eat('&') {
            parts.push(self.parse_app());
        }
        self.db.intersection_of(parts)
    }

    fn parse_app(&mut self) -> TypeId {
        let base_name = self.parse_name();
        if self.peek() == Some('<') {
            self.bump();
            let mut args = Vec::new();
            if self.peek() != Some('>') {
                args.push(self.parse_union());
                while self.eat(',') {
                    args.push(self.parse_union());
                }
            }
            self.eat('>');
            return apply_generic(self.db, &base_name, args);
        }
        self.db.nominal(&base_name)
    }

    fn parse_name(&mut self) -> String {
        self.skip();
        let mut out = String::new();
        while let Some(c) = self.peek() {
            if c.is_ascii_alphanumeric() || c == '_' || c == ':' {
                out.push(c);
                self.i += 1;
            } else {
                break;
            }
        }
        if out.is_empty() {
            "unknown".into()
        } else {
            out
        }
    }
}

fn apply_generic(db: &mut TypeDatabase, name: &str, args: Vec<TypeId>) -> TypeId {
    match name {
        "optional" | "Optional" | "Option" => db.optional(args.first().copied().unwrap_or(db.any)),
        "Result" => {
            let ok = args.first().copied().unwrap_or(db.any);
            let err = args.get(1).copied().unwrap_or(db.any);
            db.result(ok, err)
        }
        "array" | "vector" | "LuaArray" | "span" => {
            db.array(args.first().copied().unwrap_or(db.any))
        }
        "dictionary" | "map" => {
            let key = args.first().copied().unwrap_or(db.any);
            let value = args.get(1).copied().unwrap_or(db.any);
            db.map(key, value)
        }
        "signal" | "Signal" => db.signal(args),
        "func" | "function" => {
            let ret = args.first().copied().unwrap_or(db.any);
            db.function(args.iter().skip(1).copied().collect(), ret)
        }
        other => {
            let id = db.nominal(other);
            if !args.is_empty() {
                db.intern(TypeKind::Nominal(format!(
                    "{}<{}>",
                    other,
                    args.iter().map(|a| db.label(*a)).collect::<Vec<_>>().join(",")
                )));
            }
            id
        }
    }
}
