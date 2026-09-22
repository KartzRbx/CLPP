//! Interned type database. Types are identified by `TypeId`, never by copied strings.

use std::collections::HashMap;
use std::fmt;

mod parse;
pub use parse::parse_type_str;

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct TypeId(pub u32);

impl TypeId {
    pub const UNKNOWN: TypeId = TypeId(0);
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum Primitive {
    Int,
    Float,
    Bool,
    String,
}

impl Primitive {
    pub fn label(self) -> &'static str {
        match self {
            Primitive::Int => "int",
            Primitive::Float => "float",
            Primitive::Bool => "bool",
            Primitive::String => "string",
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MemberKind {
    Field,
    Method,
    Constructor,
    Property,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct FunctionType {
    pub params: Vec<TypeId>,
    pub ret: TypeId,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum TypeKind {
    Primitive(Primitive),
    /// Nominal named type (struct, Roblox class, enum). Members live in `StructInfo`.
    Nominal(String),
    Function(FunctionType),
    Array(TypeId),
    Map { key: TypeId, value: TypeId },
    Signal { params: Vec<TypeId> },
    Union(Vec<TypeId>),
    Intersection(Vec<TypeId>),
    Alias { name: String, target: TypeId },
    GenericParam {
        name: String,
        /// Bound type id when `T : Bound` (RFC 0010).
        bound: Option<TypeId>,
    },
    Any,
    Unknown,
    Never,
    Nil,
    Void,
    Auto,
}

#[derive(Clone, Debug)]
pub struct StructMember {
    pub name: String,
    pub type_id: TypeId,
    pub kind: MemberKind,
    pub params: Vec<(String, TypeId)>,
    pub is_static: bool,
    pub doc: Option<String>,
}

#[derive(Clone, Debug)]
pub struct StructInfo {
    pub name: String,
    pub type_id: TypeId,
    pub bases: Vec<TypeId>,
    pub members: Vec<StructMember>,
    pub type_params: Vec<String>,
}

#[derive(Clone, Debug)]
pub struct TypeDatabase {
    kinds: Vec<TypeKind>,
    intern_map: HashMap<TypeKind, TypeId>,
    structs: HashMap<String, StructInfo>,
    aliases: HashMap<String, TypeId>,
    pub any: TypeId,
    pub unknown: TypeId,
    pub never: TypeId,
    pub nil: TypeId,
    pub void: TypeId,
    pub auto: TypeId,
    pub int: TypeId,
    pub float: TypeId,
    pub boolean: TypeId,
    pub string: TypeId,
}

impl Default for TypeDatabase {
    fn default() -> Self {
        Self::new()
    }
}

impl TypeDatabase {
    pub fn new() -> Self {
        let mut db = Self {
            kinds: Vec::new(),
            intern_map: HashMap::new(),
            structs: HashMap::new(),
            aliases: HashMap::new(),
            any: TypeId::UNKNOWN,
            unknown: TypeId::UNKNOWN,
            never: TypeId::UNKNOWN,
            nil: TypeId::UNKNOWN,
            void: TypeId::UNKNOWN,
            auto: TypeId::UNKNOWN,
            int: TypeId::UNKNOWN,
            float: TypeId::UNKNOWN,
            boolean: TypeId::UNKNOWN,
            string: TypeId::UNKNOWN,
        };
        db.unknown = db.intern(TypeKind::Unknown);
        debug_assert_eq!(db.unknown, TypeId::UNKNOWN);
        db.any = db.intern(TypeKind::Any);
        db.never = db.intern(TypeKind::Never);
        db.nil = db.intern(TypeKind::Nil);
        db.void = db.intern(TypeKind::Void);
        db.auto = db.intern(TypeKind::Auto);
        db.int = db.intern(TypeKind::Primitive(Primitive::Int));
        db.float = db.intern(TypeKind::Primitive(Primitive::Float));
        db.boolean = db.intern(TypeKind::Primitive(Primitive::Bool));
        db.string = db.intern(TypeKind::Primitive(Primitive::String));
        db
    }

    pub fn intern(&mut self, kind: TypeKind) -> TypeId {
        let kind = self.canonicalize(kind);
        if let Some(id) = self.intern_map.get(&kind) {
            return *id;
        }
        let id = TypeId(self.kinds.len() as u32);
        self.intern_map.insert(kind.clone(), id);
        self.kinds.push(kind);
        id
    }

    /// Fresh type-parameter id (not shared across functions with the same name).
    pub fn fresh_generic(&mut self, name: &str, bound: Option<TypeId>) -> TypeId {
        let id = TypeId(self.kinds.len() as u32);
        self.kinds.push(TypeKind::GenericParam {
            name: name.to_string(),
            bound,
        });
        id
    }

    fn canonicalize(&self, kind: TypeKind) -> TypeKind {
        match kind {
            TypeKind::Union(mut parts) => {
                parts.sort_by_key(|id| id.0);
                parts.dedup();
                TypeKind::Union(parts)
            }
            TypeKind::Intersection(mut parts) => {
                parts.sort_by_key(|id| id.0);
                parts.dedup();
                TypeKind::Intersection(parts)
            }
            other => other,
        }
    }

    pub fn kind(&self, id: TypeId) -> &TypeKind {
        self.kinds
            .get(id.0 as usize)
            .unwrap_or(&TypeKind::Unknown)
    }

    pub fn nominal(&mut self, name: &str) -> TypeId {
        if let Some(id) = self.aliases.get(name) {
            return *id;
        }
        if let Some(info) = self.structs.get(name) {
            return info.type_id;
        }
        match name {
            "int" | "i32" | "i64" => self.int,
            "float" | "double" | "number" => self.float,
            "bool" | "boolean" => self.boolean,
            "string" => self.string,
            "void" => self.void,
            "auto" => self.auto,
            "any" => self.any,
            "null" | "nil" | "nullptr" => self.nil,
            "unknown" => self.unknown,
            "never" => self.never,
            other => self.intern(TypeKind::Nominal(other.to_string())),
        }
    }

    pub fn optional(&mut self, inner: TypeId) -> TypeId {
        if inner == self.nil {
            return inner;
        }
        if let TypeKind::Union(parts) = self.kind(inner).clone() {
            if parts.contains(&self.nil) {
                return inner;
            }
        }
        self.intern(TypeKind::Union(vec![inner, self.nil]))
    }

    pub fn array(&mut self, elem: TypeId) -> TypeId {
        self.intern(TypeKind::Array(elem))
    }

    pub fn map(&mut self, key: TypeId, value: TypeId) -> TypeId {
        self.intern(TypeKind::Map { key, value })
    }

    pub fn signal(&mut self, params: Vec<TypeId>) -> TypeId {
        self.intern(TypeKind::Signal { params })
    }

    pub fn function(&mut self, params: Vec<TypeId>, ret: TypeId) -> TypeId {
        self.intern(TypeKind::Function(FunctionType { params, ret }))
    }

    pub fn as_function(&self, id: TypeId) -> Option<FunctionType> {
        match self.peel(id) {
            TypeKind::Function(ft) => Some(ft),
            _ => None,
        }
    }

    pub fn union_of(&mut self, parts: Vec<TypeId>) -> TypeId {
        if parts.is_empty() {
            return self.never;
        }
        if parts.len() == 1 {
            return parts[0];
        }
        self.intern(TypeKind::Union(parts))
    }

    pub fn intersection_of(&mut self, parts: Vec<TypeId>) -> TypeId {
        if parts.is_empty() {
            return self.unknown;
        }
        if parts.len() == 1 {
            return parts[0];
        }
        self.intern(TypeKind::Intersection(parts))
    }

    pub fn define_struct(&mut self, name: &str, bases: Vec<TypeId>) -> TypeId {
        if let Some(info) = self.structs.get(name) {
            return info.type_id;
        }
        let type_id = self.intern(TypeKind::Nominal(name.to_string()));
        self.structs.insert(
            name.to_string(),
            StructInfo {
                name: name.to_string(),
                type_id,
                bases,
                members: Vec::new(),
                type_params: Vec::new(),
            },
        );
        type_id
    }

    pub fn set_bases(&mut self, name: &str, bases: Vec<TypeId>) {
        if let Some(info) = self.structs.get_mut(name) {
            info.bases = bases;
        }
    }

    pub fn add_member(&mut self, owner: &str, member: StructMember) {
        if let Some(info) = self.structs.get_mut(owner) {
            if info.members.iter().any(|m| m.name == member.name && m.kind == member.kind) {
                return;
            }
            info.members.push(member);
        }
    }

    pub fn define_alias(&mut self, name: &str, target: TypeId) -> TypeId {
        let id = self.intern(TypeKind::Alias {
            name: name.to_string(),
            target,
        });
        self.aliases.insert(name.to_string(), target);
        id
    }

    pub fn struct_info(&self, name: &str) -> Option<&StructInfo> {
        self.structs.get(name)
    }

    pub fn struct_mut(&mut self, name: &str) -> Option<&mut StructInfo> {
        self.structs.get_mut(name)
    }

    pub fn struct_of(&self, id: TypeId) -> Option<&StructInfo> {
        match self.peel(id) {
            TypeKind::Nominal(name) => self.structs.get(&name),
            _ => None,
        }
    }

    pub fn has_struct(&self, name: &str) -> bool {
        self.structs.contains_key(name)
    }

    pub fn peel(&self, id: TypeId) -> TypeKind {
        match self.kind(id) {
            TypeKind::Alias { target, .. } => self.peel(*target),
            other => other.clone(),
        }
    }

    pub fn peel_id(&self, id: TypeId) -> TypeId {
        match self.kind(id) {
            TypeKind::Alias { target, .. } => self.peel_id(*target),
            _ => id,
        }
    }

    pub fn is_nil(&self, id: TypeId) -> bool {
        matches!(self.peel(id), TypeKind::Nil)
    }

    pub fn is_unknown(&self, id: TypeId) -> bool {
        matches!(self.peel(id), TypeKind::Unknown | TypeKind::Auto | TypeKind::Any)
    }

    pub fn is_stringish(&self, id: TypeId) -> bool {
        matches!(self.peel(id), TypeKind::Primitive(Primitive::String))
    }

    pub fn is_optional(&self, id: TypeId) -> bool {
        self.unwrap_optional(id).is_some()
    }

    pub fn unwrap_optional(&self, id: TypeId) -> Option<TypeId> {
        match self.peel(id) {
            TypeKind::Union(parts) => {
                let nil = self.nil;
                let had_nil = parts.iter().any(|p| *p == nil);
                let rest: Vec<_> = parts.into_iter().filter(|p| *p != nil).collect();
                if had_nil && rest.len() == 1 {
                    Some(rest[0])
                } else {
                    None
                }
            }
            _ => None,
        }
    }

    pub fn exclude_nil(&mut self, id: TypeId) -> TypeId {
        match self.peel(id) {
            TypeKind::Union(parts) => {
                let rest: Vec<_> = parts.into_iter().filter(|p| *p != self.nil).collect();
                self.union_of(rest)
            }
            TypeKind::Nil => self.never,
            _ => id,
        }
    }

    pub fn array_elem(&self, id: TypeId) -> Option<TypeId> {
        match self.peel(id) {
            TypeKind::Array(elem) => Some(elem),
            _ => None,
        }
    }

    pub fn signal_params(&self, id: TypeId) -> Option<Vec<TypeId>> {
        match self.peel(id) {
            TypeKind::Signal { params } => Some(params),
            TypeKind::Nominal(name) if name == "RBXScriptSignal" || name == "signal" => {
                Some(Vec::new())
            }
            _ => None,
        }
    }

    pub fn is_signal(&self, id: TypeId) -> bool {
        self.signal_params(id).is_some()
    }

    pub fn is_instance_like(&self, id: TypeId) -> bool {
        let Some(info) = self.struct_of(id) else {
            return false;
        };
        if info.name == "Instance" {
            return true;
        }
        let mut seen = Vec::new();
        self.extends(id, "Instance", &mut seen)
    }

    fn extends(&self, id: TypeId, name: &str, seen: &mut Vec<TypeId>) -> bool {
        if seen.contains(&id) {
            return false;
        }
        seen.push(id);
        let Some(info) = self.struct_of(id) else {
            return false;
        };
        if info.name == name {
            return true;
        }
        info.bases
            .iter()
            .any(|base| self.extends(*base, name, seen))
    }

    pub fn label(&self, id: TypeId) -> String {
        match self.kind(id) {
            TypeKind::Primitive(p) => p.label().into(),
            TypeKind::Nominal(name) => name.clone(),
            TypeKind::Function(ft) => {
                let params: Vec<_> = ft.params.iter().map(|p| self.label(*p)).collect();
                format!("({}) -> {}", params.join(", "), self.label(ft.ret))
            }
            TypeKind::Array(elem) => format!("array<{}>", self.label(*elem)),
            TypeKind::Map { key, value } => {
                format!("dictionary<{}, {}>", self.label(*key), self.label(*value))
            }
            TypeKind::Signal { params } => {
                let inner: Vec<_> = params.iter().map(|p| self.label(*p)).collect();
                format!("signal<{}>", inner.join(", "))
            }
            TypeKind::Union(parts) => {
                if let Some(inner) = self.unwrap_optional(id) {
                    return format!("optional<{}>", self.label(inner));
                }
                parts
                    .iter()
                    .map(|p| self.label(*p))
                    .collect::<Vec<_>>()
                    .join(" | ")
            }
            TypeKind::Intersection(parts) => parts
                .iter()
                .map(|p| self.label(*p))
                .collect::<Vec<_>>()
                .join(" & "),
            TypeKind::Alias { name, .. } => name.clone(),
            TypeKind::GenericParam { name, .. } => name.clone(),
            TypeKind::Any => "any".into(),
            TypeKind::Unknown => "unknown".into(),
            TypeKind::Never => "never".into(),
            TypeKind::Nil => "null".into(),
            TypeKind::Void => "void".into(),
            TypeKind::Auto => "auto".into(),
        }
    }

    /// Members of `id`, walking inheritance (cycle-safe) and flattening intersections.
    pub fn get_members(&self, id: TypeId) -> Vec<StructMember> {
        let mut out = Vec::new();
        let mut seen_names = Vec::new();
        self.collect_members(id, &mut out, &mut seen_names, &mut Vec::new());
        out
    }

    fn collect_members(
        &self,
        id: TypeId,
        out: &mut Vec<StructMember>,
        seen_names: &mut Vec<String>,
        seen_ty: &mut Vec<TypeId>,
    ) {
        let id = self.peel_id(id);
        if seen_ty.contains(&id) {
            return;
        }
        seen_ty.push(id);
        match self.peel(id) {
            TypeKind::Nominal(_) => {
                if let Some(info) = self.struct_of(id) {
                    for member in &info.members {
                        if !seen_names.contains(&member.name) {
                            seen_names.push(member.name.clone());
                            out.push(member.clone());
                        }
                    }
                    for base in &info.bases {
                        self.collect_members(*base, out, seen_names, seen_ty);
                    }
                }
            }
            TypeKind::Intersection(parts) => {
                for part in parts {
                    self.collect_members(part, out, seen_names, seen_ty);
                }
            }
            TypeKind::GenericParam {
                bound: Some(bound),
                ..
            } => {
                self.collect_members(bound, out, seen_names, seen_ty);
            }
            TypeKind::Union(parts) => {
                if let Some(inner) = self.unwrap_optional(id) {
                    self.collect_members(inner, out, seen_names, seen_ty);
                    return;
                }
                let mut common: Option<Vec<StructMember>> = None;
                for part in parts {
                    if part == self.nil {
                        continue;
                    }
                    let members = self.get_members(part);
                    common = Some(match common {
                        None => members,
                        Some(prev) => prev
                            .into_iter()
                            .filter(|m| members.iter().any(|n| n.name == m.name))
                            .collect(),
                    });
                }
                if let Some(members) = common {
                    for member in members {
                        if !seen_names.contains(&member.name) {
                            seen_names.push(member.name.clone());
                            out.push(member);
                        }
                    }
                }
            }
            TypeKind::Signal { .. } => {
                for name in ["Connect", "Once", "Wait", "Fire"] {
                    if !seen_names.iter().any(|n| n == name) {
                        seen_names.push(name.into());
                        out.push(StructMember {
                            name: name.into(),
                            type_id: self.unknown,
                            kind: MemberKind::Method,
                            params: Vec::new(),
                            is_static: false,
                            doc: Some("signal".into()),
                        });
                    }
                }
            }
            TypeKind::Array(_) => {
                for name in ["push", "pop", "insert", "remove", "find", "len"] {
                    if !seen_names.iter().any(|n| n == name) {
                        seen_names.push(name.into());
                        out.push(StructMember {
                            name: name.into(),
                            type_id: self.unknown,
                            kind: MemberKind::Method,
                            params: Vec::new(),
                            is_static: false,
                            doc: Some("array".into()),
                        });
                    }
                }
            }
            _ => {}
        }
    }

    pub fn lookup_member(&self, id: TypeId, name: &str) -> Option<StructMember> {
        self.get_members(id)
            .into_iter()
            .find(|m| m.name == name)
    }

    /// IS-A / structural assignability for the checker (042–054).
    pub fn is_subtype(&self, from: TypeId, to: TypeId) -> bool {
        let from = self.peel_id(from);
        let to = self.peel_id(to);
        if from == to || self.is_unknown(from) || self.is_unknown(to) {
            return true;
        }
        if matches!(self.peel(to), TypeKind::Any) || matches!(self.peel(from), TypeKind::Never) {
            return true;
        }
        if matches!(self.peel(from), TypeKind::Nil) {
            return self.is_optional(to) || matches!(self.peel(to), TypeKind::Nil);
        }
        if let Some(inner) = self.unwrap_optional(from) {
            if self.is_optional(to) {
                if let Some(to_inner) = self.unwrap_optional(to) {
                    return self.is_subtype(inner, to_inner);
                }
            }
            // optional → non-optional is not a subtype
            return false;
        }
        if let Some(inner) = self.unwrap_optional(to) {
            return self.is_subtype(from, inner) || self.is_nil(from);
        }
        match (self.peel(from), self.peel(to)) {
            // Widening: int flows into float (Luau number); the reverse is a warning in typed.
            (TypeKind::Primitive(Primitive::Int), TypeKind::Primitive(Primitive::Float)) => true,
            (TypeKind::Primitive(a), TypeKind::Primitive(b)) => a == b,
            (TypeKind::Nominal(_), TypeKind::Nominal(to_name)) => {
                let mut seen = Vec::new();
                self.extends(from, &to_name, &mut seen)
            }
            (TypeKind::Array(a), TypeKind::Array(b)) => self.is_subtype(a, b),
            (
                TypeKind::Map {
                    key: ak,
                    value: av,
                },
                TypeKind::Map {
                    key: bk,
                    value: bv,
                },
            ) => self.is_subtype(ak, bk) && self.is_subtype(av, bv),
            (TypeKind::Union(parts), _) => parts.iter().all(|p| self.is_subtype(*p, to)),
            (_, TypeKind::Union(parts)) => parts.iter().any(|p| self.is_subtype(from, *p)),
            (TypeKind::Intersection(parts), _) => parts.iter().any(|p| self.is_subtype(*p, to)),
            (_, TypeKind::Intersection(parts)) => parts.iter().all(|p| self.is_subtype(from, *p)),
            (TypeKind::GenericParam { bound: Some(b), .. }, _) => self.is_subtype(b, to),
            (TypeKind::GenericParam { bound: None, .. }, _) => false,
            (_, TypeKind::GenericParam { bound: Some(b), .. }) => self.satisfies_bound(from, b),
            (_, TypeKind::GenericParam { bound: None, .. }) => true,
            _ => false,
        }
    }

    /// Concrete type meets a generic bound (nominal subtype or structural members).
    pub fn satisfies_bound(&self, concrete: TypeId, bound: TypeId) -> bool {
        if self.is_subtype(concrete, bound) {
            return true;
        }
        let bound_name = self.struct_of(bound).map(|s| s.name.clone());
        let needed: Vec<_> = self
            .get_members(bound)
            .into_iter()
            .filter(|m| {
                // Constructors / type-named members are not part of the capability surface.
                m.kind != MemberKind::Constructor
                    && bound_name.as_deref() != Some(m.name.as_str())
            })
            .collect();
        if needed.is_empty() {
            return false;
        }
        let have = self.get_members(concrete);
        needed
            .iter()
            .all(|m| have.iter().any(|h| h.name == m.name))
    }

    pub fn is_generic_param(&self, id: TypeId) -> bool {
        matches!(self.peel(id), TypeKind::GenericParam { .. })
    }

    pub fn generic_bound(&self, id: TypeId) -> Option<TypeId> {
        match self.peel(id) {
            TypeKind::GenericParam { bound, .. } => bound,
            _ => None,
        }
    }

    pub fn is_assignable(&self, value: TypeId, target: TypeId) -> bool {
        self.is_subtype(value, target)
    }
}

impl fmt::Display for TypeId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "T{}", self.0)
    }
}
