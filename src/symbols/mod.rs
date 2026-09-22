//! Symbol and scope tables. Binder output; types are filled in by the resolver.

use crate::ast::Span;
use crate::types::TypeId;
use std::collections::HashMap;

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct SymbolId(pub u32);

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, Default)]
pub struct ScopeId(pub u32);

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum SymbolKind {
    Variable,
    Function,
    Method,
    Struct,
    Field,
    Property,
    Parameter,
    TypeAlias,
    Namespace,
    Module,
    Enum,
    EnumVariant,
    Constructor,
    TypeParam,
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum ScopeKind {
    Global,
    File,
    Struct,
    Function,
    Block,
}

#[derive(Clone, Debug)]
pub struct Symbol {
    pub id: SymbolId,
    pub name: String,
    pub kind: SymbolKind,
    pub type_id: TypeId,
    pub declared_type: Option<String>,
    pub span: Span,
    pub owner: Option<SymbolId>,
    pub parent: Option<SymbolId>,
    pub scope: ScopeId,
    pub is_const: bool,
    pub is_static: bool,
    pub doc: Option<String>,
}

#[derive(Clone, Debug)]
pub struct Scope {
    pub id: ScopeId,
    pub kind: ScopeKind,
    pub parent: Option<ScopeId>,
    pub names: HashMap<String, SymbolId>,
    pub span: Span,
    pub owner: Option<SymbolId>,
}

#[derive(Clone, Debug, Default)]
pub struct SymbolDatabase {
    pub symbols: Vec<Symbol>,
    pub scopes: Vec<Scope>,
    pub file_scope: ScopeId,
}

impl SymbolDatabase {
    pub fn new() -> Self {
        let file = Scope {
            id: ScopeId(0),
            kind: ScopeKind::File,
            parent: None,
            names: HashMap::new(),
            span: Span::default(),
            owner: None,
        };
        Self {
            symbols: Vec::new(),
            scopes: vec![file],
            file_scope: ScopeId(0),
        }
    }

    pub fn alloc_scope(
        &mut self,
        kind: ScopeKind,
        parent: ScopeId,
        span: Span,
        owner: Option<SymbolId>,
    ) -> ScopeId {
        let id = ScopeId(self.scopes.len() as u32);
        self.scopes.push(Scope {
            id,
            kind,
            parent: Some(parent),
            names: HashMap::new(),
            span,
            owner,
        });
        id
    }

    pub fn alloc(
        &mut self,
        name: String,
        kind: SymbolKind,
        declared_type: Option<String>,
        span: Span,
        scope: ScopeId,
        owner: Option<SymbolId>,
    ) -> SymbolId {
        let id = SymbolId(self.symbols.len() as u32);
        self.symbols.push(Symbol {
            id,
            name: name.clone(),
            kind,
            type_id: TypeId::UNKNOWN,
            declared_type,
            span,
            owner,
            parent: None,
            scope,
            is_const: false,
            is_static: false,
            doc: None,
        });
        if let Some(sc) = self.scopes.get_mut(scope.0 as usize) {
            sc.names.insert(name, id);
        }
        id
    }

    pub fn get(&self, id: SymbolId) -> Option<&Symbol> {
        self.symbols.get(id.0 as usize)
    }

    pub fn get_mut(&mut self, id: SymbolId) -> Option<&mut Symbol> {
        self.symbols.get_mut(id.0 as usize)
    }

    pub fn scope(&self, id: ScopeId) -> Option<&Scope> {
        self.scopes.get(id.0 as usize)
    }

    pub fn lookup(&self, scope: ScopeId, name: &str) -> Option<SymbolId> {
        let mut cur = Some(scope);
        while let Some(id) = cur {
            let sc = self.scopes.get(id.0 as usize)?;
            if let Some(sym) = sc.names.get(name) {
                return Some(*sym);
            }
            cur = sc.parent;
        }
        None
    }

    pub fn lookup_at(&self, line: usize, name: &str) -> Option<SymbolId> {
        if let Some(id) = self.lookup(self.innermost_scope(line), name) {
            return Some(id);
        }
        for sc in self.scopes.iter().rev() {
            if sc.names.contains_key(name) {
                if sc.span.start_line == 0
                    || sc.span.contains_line(line)
                    || (matches!(sc.kind, ScopeKind::Function | ScopeKind::Block)
                        && line >= sc.span.start_line
                        && (sc.span.end_line == 0 || line <= sc.span.end_line.max(sc.span.start_line + 512)))
                {
                    return sc.names.get(name).copied();
                }
            }
        }
        self.lookup(self.file_scope, name)
    }

    pub fn innermost_scope(&self, line: usize) -> ScopeId {
        let mut best = self.file_scope;
        let mut best_span = usize::MAX;
        for sc in &self.scopes {
            if sc.span.start_line == 0 {
                continue;
            }
            if sc.span.contains_line(line) {
                let size = sc.span.end_line.saturating_sub(sc.span.start_line);
                if size <= best_span {
                    best_span = size;
                    best = sc.id;
                }
            }
        }
        if best == self.file_scope {
            for sc in self.scopes.iter().rev() {
                if matches!(sc.kind, ScopeKind::Function | ScopeKind::Block | ScopeKind::Struct)
                    && (sc.span.start_line == 0 || line >= sc.span.start_line)
                    && (sc.span.end_line == 0 || line <= sc.span.end_line.max(sc.span.start_line))
                {
                    return sc.id;
                }
            }
        }
        best
    }

    pub fn enclosing_owner(&self, line: usize) -> Option<SymbolId> {
        let mut cur = Some(self.innermost_scope(line));
        while let Some(id) = cur {
            let sc = self.scopes.get(id.0 as usize)?;
            if let Some(owner) = sc.owner {
                return Some(owner);
            }
            if sc.kind == ScopeKind::Function {
                if let Some(owner) = sc.owner {
                    return Some(owner);
                }
            }
            cur = sc.parent;
        }
        None
    }

    pub fn struct_named(&self, name: &str) -> Option<SymbolId> {
        self.symbols
            .iter()
            .find(|s| s.kind == SymbolKind::Struct && s.name == name)
            .map(|s| s.id)
    }

    pub fn members_of(&self, owner: SymbolId) -> Vec<SymbolId> {
        self.symbols
            .iter()
            .filter(|s| s.owner == Some(owner))
            .map(|s| s.id)
            .collect()
    }
}
