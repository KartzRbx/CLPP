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

struct FuncSig {
    params: usize,
    #[allow(dead_code)]
    line: usize,
}

pub fn check_program(program: &Program, source: &str) -> Result<Vec<CompileDiagnostic>> {
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
            Item::Destructure { .. } | Item::Unsupported { .. } => {}
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
        current_line: 1,
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
                checker.functions.insert(
                    key,
                    FuncSig {
                        params: func.params.len(),
                        line: func.line,
                    },
                );
                if func.owner.is_none() {
                    checker.functions.insert(
                        func.name.clone(),
                        FuncSig {
                            params: func.params.len(),
                            line: func.line,
                        },
                    );
                }
            }
            Item::Proto(func) => {
                let key = match &func.owner {
                    Some(owner) => format!("{owner}::{}", func.name),
                    None => func.name.clone(),
                };
                checker.functions.entry(key).or_insert(FuncSig {
                    params: func.params.len(),
                    line: func.line,
                });
                if func.owner.is_none() {
                    checker.functions.entry(func.name.clone()).or_insert(FuncSig {
                        params: func.params.len(),
                        line: func.line,
                    });
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
            _ => {}
        }
    }
    for item in &program.items {
        checker.item(item);
    }
    Ok(checker.diagnostics)
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
    current_line: usize,
}

impl<'a> Checker<'a> {
    fn item(&mut self, item: &Item) {
        match item {
            Item::Decl(decl) => self.decl(decl),
            Item::Function(func) => self.function(func),
            Item::Proto(_) => {}
            Item::Destructure { names, value } => {
                let _ = self.expr_ty(value);
                for name in names {
                    self.bind(name, Ty::Auto, false, 1, false);
                }
            }
            Item::Unsupported { .. } => {}
        }
    }

    fn function(&mut self, func: &Function) {
        let saved = self.env.clone();
        let saved_method = self.in_method;
        let saved_fields = self.method_fields.clone();
        let saved_methods = self.method_methods.clone();
        self.in_method = func.owner.is_some();
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
            Stmt::Guard { test, body } | Stmt::While { test, body } => {
                let _ = self.expr_ty(test);
                self.stmts(body);
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
                self.stmts(body);
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
                self.stmts(body);
            }
            Stmt::Switch {
                discriminant,
                cases,
            } => {
                let _ = self.expr_ty(discriminant);
                for case in cases {
                    for value in &case.values {
                        let _ = self.expr_ty(value);
                    }
                    self.stmts(&case.body);
                }
            }
            Stmt::Match {
                discriminant,
                arms,
            } => {
                let _ = self.expr_ty(discriminant);
                for arm in arms {
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
            }
            Stmt::Spawn { body, .. } | Stmt::Block(body) => self.stmts(body),
            Stmt::Return(None) | Stmt::Break => {}
        }
    }

    fn decl(&mut self, decl: &Decl) {
        let declared = declared_ty(decl);
        if let Some(value) = &decl.value {
            let actual = self.expr_ty(value);
            if !compatible(&declared, &actual) {
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
                } else if is_bare_global(name) || builtins::find(name).is_some() {
                    Ty::Unknown
                } else {
                    self.error(
                        self.current_line,
                        1,
                        format!("unknown identifier `{name}`"),
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
                    self.error(
                        *line,
                        1,
                        format!("`@{name}` is not a field or method of this struct"),
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
                    "+" | "-" | "*" | "/" => {
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
                let _ = self.expr_ty(object);
                match name.as_str() {
                    "Name" | "ClassName" | "DisplayName" | "LocaleId" => Ty::String,
                    "UserId" | "AccountAge" => Ty::Int,
                    "Character" => Ty::Named("Model".into()),
                    "Parent" => Ty::Named("Instance".into()),
                    _ => Ty::Unknown,
                }
            }
            Expr::Call { object, name, args, .. } => {
                if let Some(obj) = object {
                    let _ = self.expr_ty(obj);
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
        }
    }

    fn warn(&mut self, line: usize, column: usize, message: String) {
        let line = if line == 0 { 1 } else { line };
        self.diagnostics.push(CompileDiagnostic {
            message,
            line,
            column,
            severity: "warning".into(),
        });
    }

    fn error(&mut self, line: usize, column: usize, message: String) {
        let line = if line == 0 { 1 } else { line };
        self.diagnostics.push(CompileDiagnostic {
            message,
            line,
            column,
            severity: "error".into(),
        });
    }
}

fn declared_ty(decl: &Decl) -> Ty {
    if decl.is_observable {
        if let Some(ty) = &decl.value_type {
            let cleaned = ty.trim();
            if cleaned != "observable" {
                return parse_ty(cleaned);
            }
        }
        return Ty::Named("observable".into());
    }
    decl.value_type
        .as_deref()
        .map(parse_ty)
        .unwrap_or(Ty::Auto)
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
        return a == b;
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
