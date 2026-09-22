//! Compilation session: file cache, shared type database, compiler queries.

use crate::ast::{CompileContext, Program};
use crate::binder::{self, BoundFile};
use crate::checker::{self, Engine};
use crate::parser::{parse, parse_for_ide};
use crate::platform;
use crate::preprocess::preprocess;
use crate::symbols::SymbolDatabase;
use crate::support::CompileDiagnostic;
use crate::types::{StructMember, TypeDatabase, TypeId};
use std::collections::{HashMap, HashSet};
use std::path::Path;
use std::sync::OnceLock;

#[derive(Clone, Debug)]
pub struct CheckedFile {
    pub path: String,
    pub hash: u64,
    pub source: String,
    pub program: Program,
    pub symbols: SymbolDatabase,
    pub ctx: CompileContext,
    pub diagnostics: Vec<CompileDiagnostic>,
}

#[derive(Debug)]
pub struct Session {
    pub types: TypeDatabase,
    files: HashMap<String, CheckedFile>,
    prelude: BoundFile,
    /// Import edges for incremental invalidation (RFC 0006 lite).
    deps: HashMap<String, HashSet<String>>,
    /// Monotonic session epoch bumped on any invalidate.
    pub epoch: u64,
}

impl Default for Session {
    fn default() -> Self {
        Self::new()
    }
}

static PRELUDE: OnceLock<(TypeDatabase, BoundFile)> = OnceLock::new();

impl Session {
    /// Language-only session: no Roblox prelude. Use from tests of the core and non-Roblox hosts.
    pub fn language() -> Self {
        Self {
            types: TypeDatabase::new(),
            files: HashMap::new(),
            prelude: BoundFile {
                symbols: SymbolDatabase::new(),
            },
            deps: HashMap::new(),
            epoch: 0,
        }
    }

    /// Cluaupp / Roblox host: bind generated Instance API once, cache by process.
    pub fn with_roblox_platform() -> Self {
        let (types, prelude) = PRELUDE.get_or_init(|| {
            let mut types = TypeDatabase::new();
            let prelude = platform::load_prelude(&mut types);
            (types, prelude)
        });
        Self {
            types: types.clone(),
            files: HashMap::new(),
            prelude: prelude.clone(),
            deps: HashMap::new(),
            epoch: 0,
        }
    }

    pub fn new() -> Self {
        Self::with_roblox_platform()
    }

    pub fn check_source(&mut self, source: &str, path: &str) -> &CheckedFile {
        let hash = hash_source(source);
        if let Some(existing) = self.files.get(path) {
            if existing.hash == hash {
                // SAFETY: we return a reference after the get; split to avoid borrow issues.
            }
        }
        if self.files.get(path).is_some_and(|f| f.hash == hash) {
            return self.files.get(path).unwrap();
        }
        let file = self.build(source, path, hash);
        self.files.insert(path.to_string(), file);
        self.files.get(path).unwrap()
    }

    fn build(&mut self, source: &str, path: &str, hash: u64) -> CheckedFile {
        let file_path = Path::new(path);
        let (expanded, ctx) = preprocess(source, file_path).unwrap_or_else(|_| {
            (source.to_string(), CompileContext::default())
        });
        let (program, mut diagnostics) = match parse(&expanded, path) {
            Ok(p) => (p, Vec::new()),
            Err(_) => parse_for_ide(&expanded, path),
        };
        if diagnostics.is_empty() {
            if let Ok((_, d)) = crate::parser::parse_with_diagnostics(&expanded, path) {
                diagnostics.extend(d);
            }
        }
        let mut program = program;
        crate::ast::attach_docs(&mut program, &ctx.comments);
        if let Ok(check) = crate::checker::check_program_ex(&program, &expanded, &ctx.libraries) {
            diagnostics.extend(check);
        }
        let mut bound = binder::bind(&program);
        checker::resolve(&program, &mut bound, &mut self.types);
        diagnostics.extend(checker::check_typed(
            &program,
            &bound,
            &mut self.types,
            &expanded,
        ));
        let mut file = CheckedFile {
            path: path.to_string(),
            hash,
            source: source.to_string(),
            program,
            symbols: bound.symbols,
            ctx,
            diagnostics,
        };
        let mut seen = HashSet::new();
        crate::modules::import_requires(&mut file, &mut self.types, &mut seen);
        crate::modules::import_named(&mut file, &mut self.types, &mut seen);
        let deps: HashSet<String> = file
            .ctx
            .requires
            .iter()
            .map(|r| r.to_file.clone())
            .collect();
        self.deps.insert(path.to_string(), deps);
        let _ = &self.prelude;
        file
    }

    /// Drop a file and dependents from the cache (RFC 0006 lite).
    pub fn invalidate(&mut self, path: &str) {
        self.epoch = self.epoch.wrapping_add(1);
        let mut doomed = vec![path.to_string()];
        let mut i = 0;
        while i < doomed.len() {
            let cur = doomed[i].clone();
            i += 1;
            for (from, tos) in &self.deps {
                if tos.contains(&cur) && !doomed.contains(from) {
                    doomed.push(from.clone());
                }
            }
        }
        for p in &doomed {
            self.files.remove(p);
            self.deps.remove(p);
        }
    }

    pub fn dep_graph(&self) -> &HashMap<String, HashSet<String>> {
        &self.deps
    }

    pub fn get(&self, path: &str) -> Option<&CheckedFile> {
        self.files.get(path)
    }

    pub fn get_symbol(&self, path: &str, line: usize, name: &str) -> Option<&crate::symbols::Symbol> {
        let file = self.files.get(path)?;
        let id = file.symbols.lookup_at(line, name)?;
        file.symbols.get(id)
    }

    pub fn get_type(&mut self, path: &str, line: usize, name: &str) -> TypeId {
        let Some(file) = self.files.get(path) else {
            return TypeId::UNKNOWN;
        };
        type_at(file, &mut self.types, line, name)
    }

    pub fn get_members(&self, id: TypeId) -> Vec<StructMember> {
        self.types.get_members(id)
    }

    pub fn get_definition(
        &self,
        path: &str,
        line: usize,
        name: &str,
    ) -> Option<(String, usize, usize)> {
        let sym = self.get_symbol(path, line, name)?;
        Some((path.to_string(), sym.span.start_line.max(1), sym.span.start_col.max(1)))
    }

    pub fn get_diagnostics(&self, path: &str) -> &[CompileDiagnostic] {
        self.files
            .get(path)
            .map(|f| f.diagnostics.as_slice())
            .unwrap_or(&[])
    }
}

/// Language-service queries. The LSP and `clpp api` call this instead of re-emitting Luau.
pub struct CompilerApi<'a> {
    pub session: &'a mut Session,
}

impl<'a> CompilerApi<'a> {
    pub fn new(session: &'a mut Session) -> Self {
        Self { session }
    }

    pub fn check(&mut self, source: &str, path: &str) -> &CheckedFile {
        self.session.check_source(source, path)
    }

    pub fn get_symbol(&self, path: &str, line: usize, name: &str) -> Option<&crate::symbols::Symbol> {
        self.session.get_symbol(path, line, name)
    }

    pub fn get_type(&mut self, path: &str, line: usize, name: &str) -> TypeId {
        self.session.get_type(path, line, name)
    }

    pub fn type_of_expr(&mut self, path: &str, line: usize, prefix: &str) -> TypeId {
        let Some(file) = self.session.files.get(path) else {
            return TypeId::UNKNOWN;
        };
        type_prefix(file, &mut self.session.types, line, prefix)
    }

    pub fn get_members(&self, id: TypeId) -> Vec<StructMember> {
        self.session.get_members(id)
    }

    pub fn get_definition(
        &self,
        path: &str,
        line: usize,
        name: &str,
    ) -> Option<(String, usize, usize)> {
        self.session.get_definition(path, line, name)
    }

    pub fn get_diagnostics(&self, path: &str) -> &[CompileDiagnostic] {
        self.session.get_diagnostics(path)
    }
}

pub fn hash_source(source: &str) -> u64 {
    let mut h: u64 = 0xcbf29ce484222325;
    for b in source.as_bytes() {
        h ^= u64::from(*b);
        h = h.wrapping_mul(0x100000001b3);
    }
    h
}

/// One-shot check used by analysis and tests (owns a session internally).
pub fn analyze(source: &str, path: &str) -> (CheckedFile, TypeDatabase) {
    let mut session = Session::new();
    let _ = session.check_source(source, path);
    let file = session.files.remove(path).unwrap_or_else(|| CheckedFile {
        path: path.into(),
        hash: hash_source(source),
        source: source.into(),
        program: Program {
            items: Vec::new(),
            file_name: path.into(),
        },
        symbols: SymbolDatabase::new(),
        ctx: CompileContext::default(),
        diagnostics: Vec::new(),
    });
    (file, session.types)
}

pub fn members_of(file: &CheckedFile, types: &mut TypeDatabase, line: usize, prefix: &str) -> Vec<StructMember> {
    let mut engine = Engine {
        program: &file.program,
        symbols: &file.symbols,
        types,
        source: &file.source,
        private: std::collections::HashMap::new(),
    };
    let ty = if prefix.trim_end().ends_with('.')
        || prefix.trim_end().ends_with("~>")
        || prefix.trim_end().ends_with("::")
    {
        checker::type_of_prefix(&mut engine, line, prefix)
    } else if prefix.trim_end().ends_with('@') {
        engine.type_of_name_flow(line, "self")
    } else {
        checker::type_of_prefix(&mut engine, line, &format!("{prefix}."))
    };
    engine.get_members(ty)
}

pub fn type_at(file: &CheckedFile, types: &mut TypeDatabase, line: usize, name: &str) -> TypeId {
    let mut engine = Engine {
        program: &file.program,
        symbols: &file.symbols,
        types,
        source: &file.source,
        private: std::collections::HashMap::new(),
    };
    engine.type_of_name_flow(line, name)
}

pub fn type_prefix(file: &CheckedFile, types: &mut TypeDatabase, line: usize, prefix: &str) -> TypeId {
    let mut engine = Engine {
        program: &file.program,
        symbols: &file.symbols,
        types,
        source: &file.source,
        private: std::collections::HashMap::new(),
    };
    checker::type_of_prefix(&mut engine, line, prefix)
}
