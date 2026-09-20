use crate::ast::{Decl, Expr, Function, Item, Program, Stmt};
use crate::builtins;
use crate::semantic::is_bare_global;
use crate::support::CompileDiagnostic;
use miette::Result;
use std::collections::{HashMap, HashSet};

#[derive(Clone, Debug, PartialEq)]
enum Ty {
    Int,
    Float,
    Bool,
    String,
    Null,
    Auto,
    Func,
    Named(String),
    Unknown,
}

impl Ty {
    fn label(&self) -> String {
        match self {
            Ty::Int => "int".into(),
            Ty::Float => "float".into(),
            Ty::Bool => "bool".into(),
            Ty::String => "string".into(),
            Ty::Null => "null".into(),
            Ty::Auto => "auto".into(),
            Ty::Func => "func".into(),
            Ty::Named(name) => name.clone(),
            Ty::Unknown => "unknown".into(),
        }
    }
}

#[derive(Clone, Debug)]
struct Binding {
    ty: Ty,
    is_const: bool,
    used: bool,
    line: usize,
}

#[derive(Clone, Debug)]
#[allow(dead_code)]
struct FuncSig {
    params: usize,
    param_types: Vec<Option<String>>,
    return_type: Option<String>,
    line: usize,
}

pub fn check_program(program: &Program, source: &str) -> Result<Vec<CompileDiagnostic>> {
    check_program_ex(program, source, &[])
}

pub fn check_program_ex(
    program: &Program,
    source: &str,
    libraries: &[String],
) -> Result<Vec<CompileDiagnostic>> {
    let mut owner_fields: HashMap<String, HashSet<String>> = HashMap::new();
    let mut owner_methods: HashMap<String, HashSet<String>> = HashMap::new();
    for item in &program.items {
        match item {
            Item::Decl(decl) => {
                if let Some(owner) = &decl.owner {
                    owner_fields
                        .entry(owner.clone())
                        .or_default()
                        .insert(decl.name.clone());
                }
            }
            Item::Function(func) | Item::Proto(func) => {
                if let Some(owner) = &func.owner {
                    owner_methods
                        .entry(owner.clone())
                        .or_default()
                        .insert(func.name.clone());
                }
            }
            Item::Destructure { .. }
            | Item::Unsupported { .. }
            | Item::Enum { .. }
            | Item::TypeAlias { .. }
            | Item::Class { .. } => {}
        }
    }
    let mut checker = Checker {
        source,
        file_name: &program.file_name,
        env: HashMap::new(),
        functions: HashMap::new(),
        diagnostics: Vec::new(),
        in_method: false,
        method_fields: HashSet::new(),
        method_methods: HashSet::new(),
        owner_fields,
        owner_methods,
        owner_private: HashMap::new(),
        enums: HashMap::new(),
        aliases: HashMap::new(),
        known_types: HashSet::new(),
        deprecated: HashSet::new(),
        current_owner: None,
        libraries: libraries.to_vec(),
        current_line: 1,
        loop_depth: 0,
        in_do_while: false,
        in_callback: false,
    };
    let mut seen_fn: HashSet<String> = HashSet::new();
    let mut seen_decl: HashSet<String> = HashSet::new();
    for item in &program.items {
        match item {
            Item::Function(func) => {
                let key = match &func.owner {
                    Some(owner) => format!("{owner}::{}", func.name),
                    None => func.name.clone(),
                };
                if !seen_fn.insert(key.clone()) {
                    checker.error(
                        func.line,
                        1,
                        format!("duplicate function `{key}`"),
                    );
                }
                let sig = func_sig(func);
                checker.functions.insert(key, sig.clone());
                if func.owner.is_none() {
                    checker.functions.insert(func.name.clone(), sig);
                }
                if func.attrs.iter().any(|a| a.name == "deprecated") {
                    checker.deprecated.insert(func.name.clone());
                }
            }
            Item::Proto(func) => {
                let key = match &func.owner {
                    Some(owner) => format!("{owner}::{}", func.name),
                    None => func.name.clone(),
                };
                checker.functions.entry(key).or_insert_with(|| func_sig(func));
                if func.owner.is_none() {
                    checker
                        .functions
                        .entry(func.name.clone())
                        .or_insert_with(|| func_sig(func));
                }
                if func.attrs.iter().any(|a| a.name == "deprecated") {
                    checker.deprecated.insert(func.name.clone());
                }
            }
            Item::Decl(decl) if decl.owner.is_none() => {
                if !seen_decl.insert(decl.name.clone()) {
                    checker.error(
                        decl.line,
                        1,
                        format!("duplicate name `{}`", decl.name),
                    );
                }
            }
            Item::Unsupported { line, message, .. } => {
                checker.error(*line, 1, message.clone());
            }
            Item::Enum { name, variants, .. } => {
                checker.enums.insert(
                    name.clone(),
                    variants.iter().map(|(n, _)| n.clone()).collect(),
                );
                checker.known_types.insert(name.clone());
            }
            Item::TypeAlias { name, ty, .. } => {
                checker.aliases.insert(name.clone(), ty.clone());
                checker.known_types.insert(name.clone());
            }
            Item::Class { name, .. } => {
                checker.known_types.insert(name.clone());
            }
            _ => {}
        }
    }
    checker.collect_private(program);
    checker.check_oop(program);
    for item in &program.items {
        checker.item(item);
    }
    Ok(checker.diagnostics)
}

fn func_sig(func: &Function) -> FuncSig {
    FuncSig {
        params: func.params.len(),
        param_types: func
            .params
            .iter()
            .map(|p| p.value_type.clone())
            .collect(),
        return_type: func.return_type.clone(),
        line: func.line,
    }
}

struct Checker<'a> {
    source: &'a str,
    file_name: &'a str,
    env: HashMap<String, Binding>,
    functions: HashMap<String, FuncSig>,
    diagnostics: Vec<CompileDiagnostic>,
    in_method: bool,
    method_fields: HashSet<String>,
    method_methods: HashSet<String>,
    owner_fields: HashMap<String, HashSet<String>>,
    owner_methods: HashMap<String, HashSet<String>>,
    owner_private: HashMap<String, HashSet<String>>,
    enums: HashMap<String, Vec<String>>,
    aliases: HashMap<String, String>,
    known_types: HashSet<String>,
    deprecated: HashSet<String>,
    current_owner: Option<String>,
    libraries: Vec<String>,
    current_line: usize,
    loop_depth: usize,
    in_do_while: bool,
    in_callback: bool,
}

impl<'a> Checker<'a> {
    fn collect_private(&mut self, program: &Program) {
        for item in &program.items {
            match item {
                Item::Decl(decl) => {
                    if decl.visibility.as_deref() == Some("private") {
                        if let Some(owner) = &decl.owner {
                            self.owner_private
                                .entry(owner.clone())
                                .or_default()
                                .insert(decl.name.clone());
                        }
                    }
                    if let Some(owner) = &decl.owner {
                        self.known_types.insert(owner.clone());
                    }
                }
                Item::Function(func) | Item::Proto(func) => {
                    if func.visibility.as_deref() == Some("private") {
                        if let Some(owner) = &func.owner {
                            self.owner_private
                                .entry(owner.clone())
                                .or_default()
                                .insert(func.name.clone());
                        }
                    }
                    if let Some(owner) = &func.owner {
                        self.known_types.insert(owner.clone());
                    }
                }
                _ => {}
            }
        }
    }

    fn check_oop(&mut self, program: &Program) {
        let mut protos: HashMap<String, &Function> = HashMap::new();
        let mut defs: HashMap<String, &Function> = HashMap::new();
        let mut owners_with_defs: HashSet<String> = HashSet::new();
        let mut parents: HashMap<String, String> = HashMap::new();
        for item in &program.items {
            match item {
                Item::Function(func) => {
                    if let Some(owner) = &func.owner {
                        owners_with_defs.insert(owner.clone());
                        defs.insert(format!("{owner}::{}", func.name), func);
                    }
                }
                Item::Proto(func) => {
                    if let Some(owner) = &func.owner {
                        protos.insert(format!("{owner}::{}", func.name), func);
                    }
                }
                Item::Class {
                    name,
                    parent: Some(p),
                    ..
                } => {
                    parents.insert(name.clone(), p.clone());
                }
                _ => {}
            }
        }
        for (key, proto) in &protos {
            let Some(owner) = &proto.owner else { continue };
            if owners_with_defs.contains(owner) && !defs.contains_key(key) {
                self.diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP0601,
                    proto.line,
                    1,
                    format!("`{key}` is declared but not defined"),
                    "error",
                ));
            }
        }
        for (key, def) in &defs {
            if let Some(proto) = protos.get(key) {
                if proto.params.len() != def.params.len()
                    || !same_return(&proto.return_type, &def.return_type)
                {
                    self.diagnostics.push(crate::diag::diag(
                        crate::diag::CLPP0603,
                        def.line,
                        1,
                        format!("`{key}` does not match its declaration"),
                        "error",
                    ));
                } else {
                    for (a, b) in proto.params.iter().zip(def.params.iter()) {
                        if a.value_type.as_deref().map(normalize_ty)
                            != b.value_type.as_deref().map(normalize_ty)
                        {
                            self.diagnostics.push(crate::diag::diag(
                                crate::diag::CLPP0603,
                                def.line,
                                1,
                                format!("`{key}` parameter types differ from the declaration"),
                                "error",
                            ));
                            break;
                        }
                    }
                }
            } else {
                self.diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP0602,
                    def.line,
                    1,
                    format!("`{key}` is defined but not declared in the struct"),
                    "error",
                ));
            }
        }
        for (child, parent) in &parents {
            let Some(child_methods) = self.owner_methods.get(child) else {
                continue;
            };
            let Some(parent_methods) = self.owner_methods.get(parent) else {
                continue;
            };
            for method in child_methods {
                if parent_methods.contains(method) {
                    let ck = format!("{child}::{method}");
                    let pk = format!("{parent}::{method}");
                    if let (Some(c), Some(p)) = (self.functions.get(&ck), self.functions.get(&pk)) {
                        if c.params != p.params {
                            self.diagnostics.push(crate::diag::diag(
                                crate::diag::CLPP0603,
                                c.line,
                                1,
                                format!("`{ck}` override does not match `{pk}`"),
                                "error",
                            ));
                        }
                    }
                }
            }
        }
    }

    fn note_library_type(&mut self, raw: Option<&str>, line: usize) {
        let Some(raw) = raw else { return };
        let ty = normalize_ty(raw);
        if let Some(lib) = required_lib(&ty) {
            if !self.libraries.is_empty()
                && !self
                    .libraries
                    .iter()
                    .any(|l| l == "*" || l.eq_ignore_ascii_case(lib))
            {
                self.diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP0605,
                    line,
                    1,
                    format!("`{ty}` needs `#include` of its .clh ({lib})"),
                    "error",
                ));
            }
        }
    }

    fn check_private_member(&mut self, ty: &Ty, name: &str) {
        let Ty::Named(owner) = ty else { return };
        if self
            .owner_private
            .get(owner)
            .is_some_and(|set| set.contains(name))
            && self.current_owner.as_deref() != Some(owner.as_str())
        {
            self.diagnostics.push(crate::diag::diag(
                crate::diag::CLPP0401,
                self.current_line,
                1,
                format!("`{name}` is private on `{owner}`"),
                "error",
            ));
        }
    }

    fn check_known_member(&mut self, ty: &Ty, name: &str) {
        let Ty::Named(owner) = ty else { return };
        let fields = self.owner_fields.get(owner);
        let methods = self.owner_methods.get(owner);
        if fields.is_none() && methods.is_none() {
            return;
        }
        let in_fields = fields.is_some_and(|s| s.contains(name));
        let in_methods = methods.is_some_and(|s| s.contains(name));
        if !in_fields && !in_methods {
            let mut cands: Vec<&str> = Vec::new();
            if let Some(s) = fields {
                cands.extend(s.iter().map(|s| s.as_str()));
            }
            if let Some(s) = methods {
                cands.extend(s.iter().map(|s| s.as_str()));
            }
            let extra = crate::diag::did_you_mean(name, cands)
                .map(|h| format!("; did you mean `{h}`?"))
                .unwrap_or_default();
            self.diagnostics.push(crate::diag::diag(
                crate::diag::CLPP0604,
                self.current_line,
                1,
                format!("`{name}` is not a member of `{owner}`{extra}"),
                "error",
            ));
        }
    }

    fn check_enum_exhaustive(&mut self, ty: &Ty, covered: &[String], has_default: bool, line: usize) {
        if has_default {
            return;
        }
        let Ty::Named(name) = ty else { return };
        let Some(variants) = self.enums.get(name) else {
            return;
        };
        let missing: Vec<_> = variants
            .iter()
            .filter(|v| !covered.iter().any(|c| c == *v))
            .cloned()
            .collect();
        if !missing.is_empty() {
            self.diagnostics.push(crate::diag::diag(
                crate::diag::CLPP0501,
                line,
                1,
                format!("missing variants: {}", missing.join(", ")),
                "error",
            ));
        }
    }

    fn item(&mut self, item: &Item) {
        match item {
            Item::Decl(decl) => {
                if decl.owner.is_none() {
                    self.decl(decl);
                } else {
                    self.note_library_type(decl.value_type.as_deref(), decl.line);
                }
            }
            Item::Function(func) => self.function(func),
            Item::Proto(_) => {}
            Item::Destructure { names, value } => {
                let _ = self.expr_ty(value);
                for name in names {
                    self.bind(name, Ty::Auto, false, 1, false);
                }
            }
            Item::Unsupported { .. }
            | Item::Enum { .. }
            | Item::TypeAlias { .. }
            | Item::Class { .. } => {}
        }
    }

    fn function(&mut self, func: &Function) {
        let saved = self.env.clone();
        let saved_method = self.in_method;
        let saved_fields = self.method_fields.clone();
        let saved_methods = self.method_methods.clone();
        self.in_method = func.owner.is_some();
        self.current_owner = func.owner.clone();
        if let Some(owner) = &func.owner {
            self.method_fields = self
                .owner_fields
                .get(owner)
                .cloned()
                .unwrap_or_default();
            self.method_methods = self
                .owner_methods
                .get(owner)
                .cloned()
                .unwrap_or_default();
        } else {
            self.method_fields.clear();
            self.method_methods.clear();
        }
        for param in &func.params {
            let ty = param
                .value_type
                .as_deref()
                .map(parse_ty)
                .unwrap_or(Ty::Auto);
            self.bind(&param.name, ty, false, func.line, true);
        }
        self.current_line = func.line;
        self.stmts(&func.body);
        self.check_returns(func);
        self.warn_unused_new(&saved);
        self.env = saved;
        self.in_method = saved_method;
        self.method_fields = saved_fields;
        self.method_methods = saved_methods;
        self.current_owner = None;
        let _ = self.file_name;
        let _ = self.source;
    }

    fn needs_value_return(ret: &Option<String>) -> bool {
        match ret.as_deref().map(str::trim) {
            None | Some("") | Some("void") => false,
            _ => true,
        }
    }

    fn check_returns(&mut self, func: &Function) {
        if !Self::needs_value_return(&func.return_type) {
            return;
        }
        let ty = func.return_type.as_deref().unwrap_or("a value");
        if self.has_bare_return(&func.body) {
            self.error(
                func.line,
                1,
                format!("`{}` returns {ty} — `return;` has no value", func.name),
            );
        }
        if !self.always_returns_value(&func.body) {
            self.error(
                func.line,
                1,
                format!("`{}` must return {ty} on every path", func.name),
            );
        }
    }

    fn has_bare_return(&self, stmts: &[Stmt]) -> bool {
        stmts.iter().any(|stmt| match stmt {
            Stmt::Return(None) => true,
            Stmt::Return(_) => false,
            Stmt::If {
                consequent,
                alternate,
                ..
            } => {
                self.has_bare_return(consequent)
                    || alternate.as_ref().is_some_and(|a| self.has_bare_return(a))
            }
            Stmt::Guard { body, .. }
            | Stmt::While { body, .. }
            | Stmt::ForEach { body, .. }
            | Stmt::Spawn { body, .. }
            | Stmt::Block(body) => self.has_bare_return(body),
            Stmt::CFor { body, init, .. } => {
                self.has_bare_return(body)
                    || init.as_ref().is_some_and(|s| self.has_bare_return(std::slice::from_ref(s)))
            }
            Stmt::Match { arms, .. } => arms.iter().any(|a| self.has_bare_return(&a.body)),
            Stmt::Switch { cases, .. } => cases.iter().any(|c| self.has_bare_return(&c.body)),
            _ => false,
        })
    }

    fn always_returns_value(&self, stmts: &[Stmt]) -> bool {
        let mut ok = false;
        for stmt in stmts {
            match stmt {
                Stmt::Return(Some(_)) => return true,
                Stmt::Return(None) => return false,
                Stmt::If {
                    consequent,
                    alternate,
                    ..
                } => {
                    if let Some(alt) = alternate {
                        if self.always_returns_value(consequent) && self.always_returns_value(alt) {
                            ok = true;
                        }
                    }
                }
                Stmt::Block(body) | Stmt::Spawn { body, .. } => {
                    if self.always_returns_value(body) {
                        ok = true;
                    }
                }
                Stmt::Match { arms, .. } => {
                    if !arms.is_empty() && arms.iter().all(|a| self.always_returns_value(&a.body)) {
                        ok = true;
                    }
                }
                _ => {}
            }
        }
        ok
    }

    fn stmts(&mut self, stmts: &[Stmt]) {
        for stmt in stmts {
            self.stmt(stmt);
        }
    }

    fn stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Decl(decl) => self.decl(decl),
            Stmt::Destructure { names, value } => {
                let _ = self.expr_ty(value);
                for name in names {
                    self.bind(name, Ty::Auto, false, self.current_line, false);
                }
            }
            Stmt::Expr(expr) => {
                self.check_assign(expr);
                let _ = self.expr_ty(expr);
            }
            Stmt::Return(Some(expr)) => {
                let _ = self.expr_ty(expr);
            }
            Stmt::If {
                test,
                consequent,
                alternate,
            } => {
                let _ = self.expr_ty(test);
                self.stmts(consequent);
                if let Some(alt) = alternate {
                    self.stmts(alt);
                }
            }
            Stmt::Guard { test, body } => {
                let _ = self.expr_ty(test);
                self.stmts(body);
            }
            Stmt::While { test, body } => {
                let _ = self.expr_ty(test);
                self.loop_depth += 1;
                self.stmts(body);
                self.loop_depth -= 1;
            }
            Stmt::ForEach {
                name,
                elem_type,
                iter,
                body,
                span,
            } => {
                self.current_line = span.start_line;
                let _ = self.expr_ty(iter);
                let ty = elem_type.as_deref().map(parse_ty).unwrap_or(Ty::Auto);
                self.bind(name, ty, false, span.start_line, false);
                self.loop_depth += 1;
                self.stmts(body);
                self.loop_depth -= 1;
            }
            Stmt::CFor {
                init,
                test,
                incr,
                body,
            } => {
                if let Some(init) = init {
                    self.stmt(init);
                }
                if let Some(test) = test {
                    let _ = self.expr_ty(test);
                }
                if let Some(incr) = incr {
                    let _ = self.expr_ty(incr);
                }
                self.loop_depth += 1;
                self.stmts(body);
                self.loop_depth -= 1;
            }
            Stmt::Switch {
                discriminant,
                cases,
            } => {
                let ty = self.expr_ty(discriminant);
                let mut covered = Vec::new();
                let mut has_default = false;
                for case in cases {
                    if case.is_default {
                        has_default = true;
                    }
                    for value in &case.values {
                        if let Some(n) = enum_case_name(value) {
                            covered.push(n);
                        }
                        let _ = self.expr_ty(value);
                    }
                    self.stmts(&case.body);
                }
                self.check_enum_exhaustive(&ty, &covered, has_default, self.current_line);
            }
            Stmt::Match {
                discriminant,
                arms,
            } => {
                let ty = self.expr_ty(discriminant);
                let mut covered = Vec::new();
                let mut has_default = false;
                for arm in arms {
                    if arm.class_name.is_none() {
                        has_default = true;
                    } else if let Some(class) = &arm.class_name {
                        covered.push(class.clone());
                    }
                    if let (Some(class), Some(binding)) = (&arm.class_name, &arm.binding) {
                        self.env.insert(
                            binding.clone(),
                            Binding {
                                ty: parse_ty(class),
                                is_const: false,
                                used: false,
                                line: 1,
                            },
                        );
                    }
                    self.stmts(&arm.body);
                }
                self.check_enum_exhaustive(&ty, &covered, has_default, self.current_line);
            }
            Stmt::Spawn { body, .. } | Stmt::Block(body) => self.stmts(body),
            Stmt::DoWhile { body, test } => {
                let saved = self.in_do_while;
                self.in_do_while = true;
                self.loop_depth += 1;
                self.stmts(body);
                let _ = self.expr_ty(test);
                self.loop_depth -= 1;
                self.in_do_while = saved;
            }
            Stmt::Try { body, err_name, catch } => {
                self.stmts(body);
                self.bind(err_name, Ty::String, false, self.current_line, false);
                self.stmts(catch);
            }
            Stmt::Delay { time, body } => {
                let _ = self.expr_ty(time);
                let saved = self.in_callback;
                self.in_callback = true;
                self.stmts(body);
                self.in_callback = saved;
            }
            Stmt::Defer { body } => {
                let saved = self.in_callback;
                self.in_callback = true;
                self.stmts(body);
                self.in_callback = saved;
            }
            Stmt::FieldDestructure { names, value } => {
                let _ = self.expr_ty(value);
                for name in names {
                    self.bind(name, Ty::Auto, false, self.current_line, false);
                }
            }
            Stmt::Return(None) => {
                if self.in_callback {
                    self.warn(
                        self.current_line,
                        1,
                        "return inside delay/defer only returns from the callback".into(),
                    );
                }
            }
            Stmt::Break => {
                if self.loop_depth == 0 {
                    self.error(self.current_line, 1, "`break` is only valid inside a loop".into());
                }
            }
            Stmt::Continue => {
                if self.loop_depth == 0 {
                    self.diagnostics.push(crate::diag::diag(
                        crate::diag::CLPP0402,
                        self.current_line,
                        1,
                        "continue is only valid inside a loop",
                        "error",
                    ));
                } else if self.in_do_while {
                    self.diagnostics.push(crate::diag::diag(
                        crate::diag::CLPP0402,
                        self.current_line,
                        1,
                        "continue is not allowed in do/while",
                        "error",
                    ));
                }
            }
        }
    }

    fn decl(&mut self, decl: &Decl) {
        self.note_library_type(decl.value_type.as_deref(), decl.line);
        let declared = self.declared_ty(decl);
        if let Some(value) = &decl.value {
            let actual = self.expr_ty(value);
            if actual.label().starts_with("optional<")
                && !declared.label().starts_with("optional")
                && !matches!(declared, Ty::Auto | Ty::Unknown)
            {
                self.diagnostics.push(crate::diag::diag(
                    crate::diag::CLPP0201,
                    decl.line,
                    1,
                    format!(
                        "cannot assign {} to '{}'",
                        actual.label(),
                        format_decl_name(decl)
                    ),
                    "error",
                ));
            } else if matches!(declared, Ty::Int) && matches!(actual, Ty::Float) {
                self.warn(
                    decl.line,
                    1,
                    format!(
                        "initializing int '{}' from float; Luau does not truncate",
                        decl.name
                    ),
                );
            } else if !compatible(&declared, &actual) {
                self.error(
                    decl.line,
                    1,
                    format!(
                        "cannot initialize '{}' with {}; expected {}",
                        format_decl_name(decl),
                        actual.label(),
                        declared.label()
                    ),
                );
            }
        }
        self.env.insert(
            decl.name.clone(),
            Binding {
                ty: declared,
                is_const: decl.is_const,
                used: false,
                line: decl.line,
            },
        );
    }

    fn bind(&mut self, name: &str, ty: Ty, is_const: bool, line: usize, is_param: bool) {
        if let Some(prev) = self.env.get(name) {
            if prev.line != line {
                self.warn(
                    line,
                    1,
                    format!("`{name}` shadows a previous binding"),
                );
            }
        }
        let _ = is_param;
        self.env.insert(
            name.to_string(),
            Binding {
                ty,
                is_const,
                used: false,
                line,
            },
        );
    }

    fn warn_unused_new(&mut self, saved: &HashMap<String, Binding>) {
        let unused: Vec<(String, usize)> = self
            .env
            .iter()
            .filter(|(name, b)| {
                !saved.contains_key(*name)
                    && !b.used
                    && *name != "_"
                    && !name.starts_with('_')
                    && !name.eq_ignore_ascii_case("janitor")
            })
            .map(|(n, b)| (n.clone(), b.line))
            .collect();
        for (name, line) in unused {
            self.warn(line, 1, format!("unused `{name}`"));
        }
    }

    fn check_assign(&mut self, expr: &Expr) {
        if let Expr::Assign { left, right, .. } = expr {
            if let Expr::Ident(name) = left.as_ref() {
                let binding = self.env.get(name).map(|b| (b.is_const, b.ty.clone()));
                if let Some((is_const, expected)) = binding {
                    let line = match expr {
                        Expr::Assign { line, .. } => *line,
                        _ => 1,
                    };
                    if is_const {
                        self.error(line, 1, format!("cannot assign to const '{name}'"));
                    }
                    let actual = self.expr_ty(right);
                    if !compatible(&expected, &actual) {
                        self.error(
                            line,
                            1,
                            format!(
                                "cannot assign {} to '{name}'; expected {}",
                                actual.label(),
                                expected.label()
                            ),
                        );
                    }
                }
            }
        }
    }

    fn expr_ty(&mut self, expr: &Expr) -> Ty {
        match expr {
            Expr::Null => Ty::Null,
            Expr::Bool(_) => Ty::Bool,
            Expr::Number(n) => {
                if n.contains('.') {
                    Ty::Float
                } else {
                    Ty::Int
                }
            }
            Expr::String(_) | Expr::Interp { .. } => Ty::String,
            Expr::Ident(name) => {
                if let Some(binding) = self.env.get_mut(name) {
                    binding.used = true;
                    binding.ty.clone()
                } else if name == "this" || name == "self" {
                    if !self.in_method {
                        self.error(
                            self.current_line,
                            1,
                            "`this` is only valid inside Class::Method".into(),
                        );
                    }
                    Ty::Auto
                } else if self.in_method && self.method_fields.contains(name) {
                    self.diagnostics.push(crate::diag::diag(
                        crate::diag::CLPP0102,
                        self.current_line,
                        1,
                        format!("field `{name}` needs `@`"),
                        "error",
                    ));
                    Ty::Unknown
                } else if is_bare_global(name) || builtins::find(name).is_some() {
                    Ty::Unknown
                } else {
                    let hint = crate::diag::did_you_mean(
                        name,
                        self.env.keys().map(|s| s.as_str()),
                    );
                    let extra = hint
                        .map(|h| format!("; did you mean `{h}`?"))
                        .unwrap_or_default();
                    self.error(
                        self.current_line,
                        1,
                        format!("unknown identifier `{name}`{extra}"),
                    );
                    Ty::Unknown
                }
            }
            Expr::Tuple(values) => values
                .last()
                .map(|v| self.expr_ty(v))
                .unwrap_or(Ty::Unknown),
            Expr::This { line } => {
                if !self.in_method {
                    self.error(
                        *line,
                        1,
                        "`@this` is only valid inside Class::Method".into(),
                    );
                }
                Ty::Auto
            }
            Expr::AtField { name, line } => {
                if !self.in_method {
                    self.error(
                        *line,
                        1,
                        format!("`@{name}` is only valid inside Class::Method"),
                    );
                } else if !self.method_fields.contains(name) && !self.method_methods.contains(name)
                {
                    let extra = crate::diag::did_you_mean(
                        name,
                        self.method_fields
                            .iter()
                            .chain(self.method_methods.iter())
                            .map(|s| s.as_str()),
                    )
                    .map(|h| format!("; did you mean `@{h}`?"))
                    .unwrap_or_default();
                    self.error(
                        *line,
                        1,
                        format!("`@{name}` is not a field or method of this struct{extra}"),
                    );
                }
                Ty::Unknown
            }
            Expr::Unary { argument, .. } | Expr::Await { argument } | Expr::Cast { argument, .. } => {
                if let Expr::Cast { value_type, .. } = expr {
                    return parse_ty(value_type);
                }
                self.expr_ty(argument)
            }
            Expr::Binary { op, left, right } => {
                let lt = self.expr_ty(left);
                let rt = self.expr_ty(right);
                match op.as_str() {
                    ".:" => Ty::String,
                    "+" | "-" | "*" | "/" | "%" => {
                        if op == "/" && matches!(lt, Ty::Int) && matches!(rt, Ty::Int) {
                            self.warn(
                                self.current_line,
                                1,
                                "int / int is Luau `/` (float); use div(a, b) for `//`".into(),
                            );
                        }
                        if matches!(lt, Ty::Float) || matches!(rt, Ty::Float) {
                            Ty::Float
                        } else {
                            Ty::Int
                        }
                    }
                    "==" | "!=" | "<" | ">" | "<=" | ">=" | "&&" | "||" => Ty::Bool,
                    _ => Ty::Unknown,
                }
            }
            Expr::Assign { left, right, .. } => {
                let _ = self.expr_ty(left);
                self.expr_ty(right)
            }
            Expr::Member { object, name, .. } => {
                let obj_ty = self.expr_ty(object);
                self.check_private_member(&obj_ty, name);
                self.check_known_member(&obj_ty, name);
                match name.as_str() {
                    "Name" | "ClassName" | "DisplayName" | "LocaleId" => Ty::String,
                    "UserId" | "AccountAge" => Ty::Int,
                    "Character" => Ty::Named("Model".into()),
                    "Parent" => Ty::Named("Instance".into()),
                    _ => Ty::Unknown,
                }
            }
            Expr::Call { object, name, args, .. } => {
                if object.is_none()
                    && self.in_method
                    && self.method_methods.contains(name)
                    && !self.env.contains_key(name)
                {
                    self.diagnostics.push(crate::diag::diag(
                        crate::diag::CLPP0101,
                        self.current_line,
                        1,
                        format!("method `{name}` needs `@`"),
                        "error",
                    ));
                }
                if self.deprecated.contains(name) {
                    self.warn(
                        self.current_line,
                        1,
                        format!("`{name}` is [[deprecated]]"),
                    );
                }
                if name == "static_assert" {
                    match args.first() {
                        Some(Expr::Bool(false)) => {
                            self.diagnostics.push(crate::diag::diag(
                                crate::diag::CLPP0701,
                                self.current_line,
                                1,
                                "static_assert condition is false",
                                "error",
                            ));
                        }
                        Some(Expr::Number(n)) if n == "0" => {
                            self.diagnostics.push(crate::diag::diag(
                                crate::diag::CLPP0701,
                                self.current_line,
                                1,
                                "static_assert condition is false",
                                "error",
                            ));
                        }
                        Some(Expr::Bool(true)) | Some(Expr::Number(_)) => {}
                        Some(_) => {
                            self.diagnostics.push(crate::diag::diag(
                                crate::diag::CLPP0701,
                                self.current_line,
                                1,
                                "static_assert needs a constant bool or integer",
                                "error",
                            ));
                        }
                        None => {
                            self.diagnostics.push(crate::diag::diag(
                                crate::diag::CLPP0701,
                                self.current_line,
                                1,
                                "static_assert needs a condition",
                                "error",
                            ));
                        }
                    }
                }
                if let Some(obj) = object {
                    let obj_ty = self.expr_ty(obj);
                    self.check_private_member(&obj_ty, name);
                    self.check_known_member(&obj_ty, name);
                } else if let Some(binding) = self.env.get_mut(name) {
                    binding.used = true;
                } else if let Some(sig) = self.functions.get(name) {
                    if args.len() != sig.params {
                        self.error(
                            self.current_line,
                            1,
                            format!(
                                "`{name}` takes {} argument(s), got {}",
                                sig.params,
                                args.len()
                            ),
                        );
                    }
                } else if builtins::find(name).is_some() && !builtins::arity_ok(name, args.len()) {
                    self.error(
                        self.current_line,
                        1,
                        format!("wrong number of arguments to `{name}`"),
                    );
                } else if !is_bare_global(name)
                    && builtins::find(name).is_none()
                    && !self.method_methods.contains(name)
                    && !crate::semantic::is_datatype(name)
                {
                    self.error(
                        self.current_line,
                        1,
                        format!("unknown identifier `{name}`"),
                    );
                }
                for arg in args {
                    let _ = self.expr_ty(arg);
                }
                match name.as_str() {
                    "FindFirstChild" | "FindFirstChildOfClass" | "FindFirstChildWhichIsA" => {
                        Ty::Named("optional<Instance>".into())
                    }
                    "WaitForChild" => Ty::Named("Instance".into()),
                    "GetService" => Ty::Named(
                        args.first()
                            .and_then(|a| match a {
                                Expr::String(s) => Some(s.clone()),
                                _ => None,
                            })
                            .unwrap_or_else(|| "Instance".into()),
                    ),
                    "Fire" | "Destroy" | "Kick" => Ty::Auto,
                    "to_string" | "tostring" => Ty::String,
                    "to_number" | "tonumber" => Ty::Float,
                    "to_bool" => Ty::Bool,
                    "Connect" | "Once" => Ty::Named("RBXScriptConnection".into()),
                    "GetPropertyChangedSignal" => Ty::Named("RBXScriptSignal".into()),
                    _ => Ty::Unknown,
                }
            }
            Expr::GetService { service } => Ty::Named(service.clone()),
            Expr::New { class_name, args } => {
                for arg in args {
                    let _ = self.expr_ty(arg);
                }
                Ty::Named(class_name.clone())
            }
            Expr::Lambda { params, body } => {
                let saved = self.env.clone();
                for param in params {
                    let ty = param
                        .value_type
                        .as_deref()
                        .map(parse_ty)
                        .unwrap_or(Ty::Auto);
                    self.bind(&param.name, ty, false, self.current_line, true);
                }
                self.stmts(body);
                self.env = saved;
                Ty::Func
            }
            Expr::InitList { .. } | Expr::ArrayLit { .. } => Ty::Named("array".into()),
            Expr::DictLit { .. } => Ty::Named("dictionary".into()),
            Expr::Update { target, .. } => self.expr_ty(target),
            Expr::Index { object, index } => {
                let _ = self.expr_ty(object);
                let _ = self.expr_ty(index);
                Ty::Unknown
            }
            Expr::OptionalChain { object, .. } => self.expr_ty(object),
            Expr::Coalesce { left, right } => {
                let _ = self.expr_ty(left);
                self.expr_ty(right)
            }
            Expr::Ternary {
                cond,
                then_expr,
                else_expr,
            } => {
                let _ = self.expr_ty(cond);
                let t = self.expr_ty(then_expr);
                let _ = self.expr_ty(else_expr);
                t
            }
        }
    }

    fn declared_ty(&self, decl: &Decl) -> Ty {
        if decl.is_observable {
            if let Some(ty) = &decl.value_type {
                let cleaned = ty.trim();
                if cleaned != "observable" {
                    return self.resolve_ty(cleaned);
                }
            }
            return Ty::Named("observable".into());
        }
        decl.value_type
            .as_deref()
            .map(|t| self.resolve_ty(t))
            .unwrap_or(Ty::Auto)
    }

    fn resolve_ty(&self, raw: &str) -> Ty {
        let cleaned = normalize_ty(raw);
        if let Some(alias) = self.aliases.get(&cleaned) {
            return parse_ty(alias);
        }
        parse_ty(raw)
    }

    fn warn(&mut self, line: usize, column: usize, message: String) {
        let line = if line == 0 { 1 } else { line };
        self.diagnostics.push(CompileDiagnostic {
            message,
            line,
            column,
            severity: "warning".into(),
            code: None,
            help: None,
        });
    }

    fn error(&mut self, line: usize, column: usize, message: String) {
        let line = if line == 0 { 1 } else { line };
        self.diagnostics.push(CompileDiagnostic {
            message,
            line,
            column,
            severity: "error".into(),
            code: None,
            help: None,
        });
    }
}

fn parse_ty(raw: &str) -> Ty {
    let cleaned = raw
        .trim()
        .trim_start_matches("const ")
        .trim_end_matches('*')
        .trim();
    if cleaned.starts_with("observable ") {
        return parse_ty(cleaned.trim_start_matches("observable "));
    }
    if cleaned.starts_with("signal<") || cleaned == "signal" {
        return Ty::Named("signal".into());
    }
    if cleaned.starts_with("array<") || cleaned.starts_with("vector<") || cleaned == "array" {
        return Ty::Named("array".into());
    }
    if cleaned.starts_with("dictionary<") || cleaned.starts_with("map<") || cleaned == "dictionary" {
        return Ty::Named("dictionary".into());
    }
    match cleaned {
        "int" => Ty::Int,
        "float" | "double" => Ty::Float,
        "bool" => Ty::Bool,
        "string" => Ty::String,
        "auto" => Ty::Auto,
        "func" => Ty::Func,
        "void" => Ty::Auto,
        other => Ty::Named(other.to_string()),
    }
}

fn compatible(expected: &Ty, actual: &Ty) -> bool {
    if matches!(expected, Ty::Auto | Ty::Unknown) || matches!(actual, Ty::Unknown | Ty::Auto) {
        return true;
    }
    if actual == expected {
        return true;
    }
    if matches!(actual, Ty::Null) {
        return !matches!(expected, Ty::Int | Ty::Float | Ty::Bool | Ty::String);
    }
    if matches!(expected, Ty::Float) && matches!(actual, Ty::Int) {
        return true;
    }
    if let (Ty::Named(a), Ty::Named(b)) = (expected, actual) {
        if a == b {
            return true;
        }
        if crate::semantic::is_instance_type(a) && crate::semantic::is_instance_type(b) {
            return true;
        }
    }
    false
}

fn format_decl_name(decl: &Decl) -> String {
    let ty = decl.value_type.as_deref().unwrap_or("auto");
    if decl.is_const {
        format!("const {ty} {}", decl.name)
    } else if decl.is_observable {
        format!("observable {ty} {}", decl.name)
    } else {
        format!("{ty} {}", decl.name)
    }
}

fn normalize_ty(raw: &str) -> String {
    raw.trim()
        .trim_start_matches("const ")
        .trim_end_matches('*')
        .trim()
        .to_string()
}

fn required_lib(ty: &str) -> Option<&'static str> {
    match ty {
        "Janitor" | "Maid" => Some("Janitor"),
        "DataService" | "DataServiceServer" | "DataServiceClient" | "Data" => Some("DataService"),
        "Signal" => Some("Spark"),
        "Promise" => Some("Promise"),
        _ => None,
    }
}

fn same_return(a: &Option<String>, b: &Option<String>) -> bool {
    fn norm(v: &Option<String>) -> Option<String> {
        v.as_deref()
            .map(normalize_ty)
            .filter(|s| !s.is_empty() && s != "void")
    }
    norm(a) == norm(b)
}

fn enum_case_name(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Ident(name) => Some(name.clone()),
        Expr::Member { name, .. } => Some(name.clone()),
        Expr::Call {
            object: Some(obj),
            name,
            args,
            ..
        } if args.is_empty() => match obj.as_ref() {
            Expr::Ident(_) => Some(name.clone()),
            _ => None,
        },
        _ => None,
    }
}
