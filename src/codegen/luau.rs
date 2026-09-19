use crate::ast::{CompileContext, Decl, Expr, Function, Item, MatchArm, Program, Stmt, SwitchCase};
use crate::semantic::{
    is_array_type, is_bare_global, is_datatype, is_instance_type, is_library_type, is_luau_lib,
    is_method, is_signal_type, luau_type, observable_class,
};
use std::collections::HashSet;
use std::path::{Path, PathBuf};

pub fn emit(program: &Program, ctx: &CompileContext) -> String {
    let mut e = Emitter::new(program, ctx);
    e.emit_program();
    e.lines.join("\n")
}

struct Emitter<'a> {
    program: &'a Program,
    ctx: &'a CompileContext,
    lines: Vec<String>,
    class_owners: HashSet<String>,
    class_fields: HashSet<String>,
    class_methods: HashSet<String>,
    nested_types: HashSet<String>,
    local_names: HashSet<String>,
    observables: HashSet<String>,
    needs_obs_table: bool,
    fresh_values: HashSet<String>,
    self_owner: Option<String>,
    switch_id: usize,
    match_id: usize,
}

impl<'a> Emitter<'a> {
    fn new(program: &'a Program, ctx: &'a CompileContext) -> Self {
        let mut class_owners = HashSet::new();
        let mut class_fields = HashSet::new();
        let mut class_methods = HashSet::new();
        let mut observables = HashSet::new();
        for item in &program.items {
            match item {
                Item::Decl(decl) => {
                    if let Some(owner) = &decl.owner {
                        class_owners.insert(owner.clone());
                        class_fields.insert(decl.name.clone());
                    }
                    if decl.is_observable {
                        observables.insert(decl.name.clone());
                    }
                }
                Item::Function(func) | Item::Proto(func) => {
                    if let Some(owner) = &func.owner {
                        class_owners.insert(owner.clone());
                        class_methods.insert(func.name.clone());
                    }
                }
                Item::Destructure { .. } => {}
            }
        }
        let mut nested_types = HashSet::new();
        for item in &program.items {
            if let Item::Decl(decl) = item {
                if let (Some(owner), Some(ty)) = (&decl.owner, &decl.value_type) {
                    if class_owners.contains(ty) && ty != owner {
                        nested_types.insert(ty.clone());
                    }
                }
            }
        }
        Self {
            program,
            ctx,
            lines: Vec::new(),
            class_owners,
            class_fields,
            class_methods,
            nested_types,
            local_names: HashSet::new(),
            needs_obs_table: !observables.is_empty(),
            observables,
            fresh_values: HashSet::new(),
            self_owner: None,
            switch_id: 0,
            match_id: 0,
        }
    }

    fn emit_program(&mut self) {
        if self.ctx.strict {
            self.lines.push("--!strict".into());
        } else if self.ctx.nonstrict {
            self.lines.push("--!nonstrict".into());
        }
        if self.ctx.native {
            self.lines.push("--!native".into());
        }
        if let Some(level) = self.ctx.optimize {
            self.lines.push(format!("--!optimize {level}"));
        }
        self.lines.push("-- Compiled by CL++ — C++ × Luau".into());
        self.lines.push(String::new());
        for comment in &self.ctx.comments {
            self.lines.push(comment.clone());
        }
        if !self.ctx.comments.is_empty() {
            self.lines.push(String::new());
        }

        self.emit_requires();
        self.emit_runtime_helpers();

        let functions: Vec<&Function> = self
            .program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(f) => Some(f),
                _ => None,
            })
            .collect();
        let has_owned_functions = functions.iter().any(|f| f.owner.is_some());
        let class_module = !functions.is_empty() && functions.iter().all(|f| f.owner.is_some());
        let struct_roots: Vec<String> = self
            .class_owners
            .iter()
            .filter(|name| !self.nested_types.contains(*name))
            .cloned()
            .collect();
        let has_protos = self
            .program
            .items
            .iter()
            .any(|item| matches!(item, Item::Proto(f) if f.owner.is_some()));
        let only_structs = functions.is_empty()
            && !has_protos
            && !struct_roots.is_empty()
            && self
                .program
                .items
                .iter()
                .all(|item| matches!(item, Item::Decl(_) | Item::Proto(_)));

        if self.ctx.is_header {
            self.emit_header_type(&struct_roots);
            for root in &struct_roots {
                self.emit_struct_ctor(root);
            }
            if struct_roots.len() == 1 {
                self.lines.push(format!("return {}", struct_roots[0]));
            } else if !struct_roots.is_empty() {
                self.lines.push("return {".into());
                for root in &struct_roots {
                    self.lines.push(format!("\t{root} = {root},"));
                }
                self.lines.push("}".into());
            } else {
                self.lines.push("return {}".into());
            }
            self.lines.push(String::new());
            return;
        }

        if only_structs {
            for root in &struct_roots {
                self.emit_struct_ctor(root);
            }
            if struct_roots.len() == 1 {
                self.lines.push(format!("return {}", struct_roots[0]));
            } else {
                self.lines.push("return {".into());
                for root in &struct_roots {
                    self.lines.push(format!("\t{root} = {root},"));
                }
                self.lines.push("}".into());
            }
            self.lines.push(String::new());
            return;
        }

        if has_owned_functions {
            for owner in self.sorted_owners() {
                self.lines.push(format!("local {owner} = {{}}"));
                self.lines.push(String::new());
            }
        }

        let const_only = functions.is_empty()
            && self
                .program
                .items
                .iter()
                .all(|item| matches!(item, Item::Decl(d) if d.owner.is_none()));

        for item in &self.program.items {
            match item {
                Item::Decl(decl) => {
                    let lines = self.emit_decl(decl, 0);
                    self.lines.extend(lines);
                    self.lines.push(String::new());
                }
                Item::Destructure { names, value } => {
                    let lines = self.emit_destructure(names, value, 0);
                    self.lines.extend(lines);
                    self.lines.push(String::new());
                }
                Item::Function(func) => {
                    if self.should_emit_function(func) {
                        self.emit_function(func);
                    }
                }
                Item::Proto(_) => {}
            }
        }

        let has_init = functions.iter().any(|f| f.name == "init" && f.owner.is_none());
        if class_module && !self.ctx.is_script {
            for owner in self.sorted_owners() {
                self.lines.push(format!("return {owner}"));
                self.lines.push(String::new());
            }
        } else if has_init && self.ctx.is_script {
            self.lines.push("init()".into());
            self.lines.push(String::new());
        } else if const_only {
            self.lines.push("return {".into());
            for item in &self.program.items {
                if let Item::Decl(decl) = item {
                    self.lines
                        .push(format!("\t{} = {},", decl.name, decl.name));
                }
            }
            self.lines.push("}".into());
            self.lines.push(String::new());
        }
    }

    fn sorted_owners(&self) -> Vec<String> {
        let mut owners: Vec<_> = self.class_owners.iter().cloned().collect();
        owners.sort();
        owners
    }

    fn emit_requires(&mut self) {
        let mut libs = self.ctx.libraries.clone();
        if program_uses_cleanup(self.program) && !libs.iter().any(|l| l == "Janitor" || l == "*") {
            libs.push("Janitor".into());
        }
        if libs.iter().any(|l| l == "*") {
            libs.retain(|l| l != "*");
            for name in ["Janitor", "DataService", "Promise", "Net"] {
                if self.uses_ident(name) && !libs.iter().any(|l| l == name) {
                    libs.push(name.into());
                }
            }
        }
        libs.sort();
        libs.dedup();
        for lib in &libs {
            self.lines
                .push(format!("const {lib} = require(ClppLibs.{lib})"));
        }
        for req in &self.ctx.requires {
            let expr = rojo_require(&req.from_file, &req.to_file);
            self.lines
                .push(format!("const {} = require({expr})", req.name));
        }
        if !libs.is_empty() || !self.ctx.requires.is_empty() {
            self.lines.push(String::new());
        }
    }

    fn emit_runtime_helpers(&mut self) {
        let mut injected = false;
        if program_uses_await(self.program) {
            self.lines.push("local function __await(value)".into());
            self.lines.push("\tif typeof(value) == \"table\" and typeof(value.expect) == \"function\" then".into());
            self.lines.push("\t\treturn value:expect()".into());
            self.lines.push("\tend".into());
            self.lines.push("\treturn value".into());
            self.lines.push("end".into());
            self.lines.push(String::new());
            injected = true;
        }
        if program_uses_signal(self.program) {
            self.lines.push("local function __signal()".into());
            self.lines.push("\tlocal bindable = Instance.new(\"BindableEvent\")".into());
            self.lines.push("\tlocal sig = {}".into());
            self.lines.push("\tfunction sig:Connect(handler)".into());
            self.lines.push("\t\treturn bindable.Event:Connect(handler)".into());
            self.lines.push("\tend".into());
            self.lines.push("\tfunction sig:Once(handler)".into());
            self.lines.push("\t\treturn bindable.Event:Once(handler)".into());
            self.lines.push("\tend".into());
            self.lines.push("\tfunction sig:Wait()".into());
            self.lines.push("\t\treturn bindable.Event:Wait()".into());
            self.lines.push("\tend".into());
            self.lines.push("\tfunction sig:Fire(...)".into());
            self.lines.push("\t\tbindable:Fire(...)".into());
            self.lines.push("\tend".into());
            self.lines.push("\tfunction sig:Destroy()".into());
            self.lines.push("\t\tbindable:Destroy()".into());
            self.lines.push("\tend".into());
            self.lines.push("\treturn sig".into());
            self.lines.push("end".into());
            self.lines.push(String::new());
            injected = true;
        }
        if self.needs_obs_table {
            self.lines
                .push("local __clpp_obs = setmetatable({}, { __mode = \"k\" })".into());
            self.lines.push(String::new());
            injected = true;
        }
        let _ = injected;
    }

    fn should_emit_function(&self, func: &Function) -> bool {
        match (func.target.as_deref(), self.ctx.script_kind.as_deref()) {
            (Some("server"), Some("client")) => false,
            (Some("client"), Some("server")) => false,
            _ => true,
        }
    }

    fn uses_ident(&self, name: &str) -> bool {
        fn walk_expr(expr: &Expr, name: &str) -> bool {
            match expr {
                Expr::Ident(n) => n == name,
                Expr::Tuple(values) => values.iter().any(|v| walk_expr(v, name)),
                Expr::Unary { argument, .. }
                | Expr::Cast { argument, .. }
                | Expr::Await { argument } => walk_expr(argument, name),
                Expr::Binary { left, right, .. } | Expr::Assign { left, right, .. } => {
                    walk_expr(left, name) || walk_expr(right, name)
                }
                Expr::Member { object, .. } => walk_expr(object, name),
                Expr::Call { object, args, .. } => {
                    object.as_ref().is_some_and(|o| walk_expr(o, name))
                        || args.iter().any(|a| walk_expr(a, name))
                }
                Expr::New { args, class_name } => {
                    class_name == name || args.iter().any(|a| walk_expr(a, name))
                }
                Expr::Lambda { body, .. } => body.iter().any(|s| walk_stmt(s, name)),
                Expr::InitList { fields } => fields.iter().any(|(_, v)| walk_expr(v, name)),
                Expr::ArrayLit { elements } => elements.iter().any(|v| walk_expr(v, name)),
                Expr::DictLit { pairs } => {
                    pairs
                        .iter()
                        .any(|(k, v)| walk_expr(k, name) || walk_expr(v, name))
                }
                Expr::Update { target, .. } => walk_expr(target, name),
                Expr::Interp { parts } => parts.iter().any(|part| match part {
                    crate::ast::InterpPart::Value(expr) => walk_expr(expr, name),
                    crate::ast::InterpPart::Text(_) => false,
                }),
                _ => false,
            }
        }
        fn walk_stmt(stmt: &Stmt, name: &str) -> bool {
            match stmt {
                Stmt::Expr(expr) | Stmt::Return(Some(expr)) => walk_expr(expr, name),
                Stmt::Decl(decl) => decl.value.as_ref().is_some_and(|v| walk_expr(v, name)),
                Stmt::Destructure { value, .. } => walk_expr(value, name),
                Stmt::Guard { test, body } => {
                    walk_expr(test, name) || body.iter().any(|s| walk_stmt(s, name))
                }
                Stmt::Match {
                    discriminant,
                    arms,
                } => {
                    walk_expr(discriminant, name)
                        || arms.iter().any(|a| a.body.iter().any(|s| walk_stmt(s, name)))
                }
                Stmt::Spawn { body, .. } => body.iter().any(|s| walk_stmt(s, name)),
                Stmt::If {
                    test,
                    consequent,
                    alternate,
                } => {
                    walk_expr(test, name)
                        || consequent.iter().any(|s| walk_stmt(s, name))
                        || alternate
                            .as_ref()
                            .is_some_and(|a| a.iter().any(|s| walk_stmt(s, name)))
                }
                Stmt::While { test, body } => {
                    walk_expr(test, name) || body.iter().any(|s| walk_stmt(s, name))
                }
                Stmt::ForEach { iter, body, .. } => {
                    walk_expr(iter, name) || body.iter().any(|s| walk_stmt(s, name))
                }
                Stmt::CFor {
                    init,
                    test,
                    incr,
                    body,
                } => {
                    init.as_ref().is_some_and(|s| walk_stmt(s, name))
                        || test.as_ref().is_some_and(|e| walk_expr(e, name))
                        || incr.as_ref().is_some_and(|e| walk_expr(e, name))
                        || body.iter().any(|s| walk_stmt(s, name))
                }
                Stmt::Switch {
                    discriminant,
                    cases,
                } => {
                    walk_expr(discriminant, name)
                        || cases.iter().any(|c| {
                            c.values.iter().any(|v| walk_expr(v, name))
                                || c.body.iter().any(|s| walk_stmt(s, name))
                        })
                }
                Stmt::Block(body) => body.iter().any(|s| walk_stmt(s, name)),
                _ => false,
            }
        }
        self.program.items.iter().any(|item| match item {
            Item::Decl(decl) => {
                decl.value_type.as_deref() == Some(name)
                    || decl.value.as_ref().is_some_and(|v| walk_expr(v, name))
            }
            Item::Function(func) | Item::Proto(func) => {
                func.body.iter().any(|s| walk_stmt(s, name))
            }
            Item::Destructure { value, .. } => walk_expr(value, name),
        })
    }

    fn emit_header_type(&mut self, roots: &[String]) {
        for root in roots {
            self.lines.push(format!("export type {root} = {{"));
            for item in &self.program.items {
                match item {
                    Item::Decl(decl) if decl.owner.as_deref() == Some(root.as_str()) => {
                        let ty = luau_type(decl.value_type.as_deref()).unwrap_or_else(|| "any".into());
                        self.lines.push(format!("\t{}: {ty},", decl.name));
                    }
                    Item::Proto(func) | Item::Function(func)
                        if func.owner.as_deref() == Some(root.as_str()) =>
                    {
                        let params = self.param_list(func);
                        let extra = if params.is_empty() { String::new() } else { format!(", {params}") };
                        let ret = match luau_type(func.return_type.as_deref()) {
                            Some(ty) if ty != "()" => format!(" -> {ty}"),
                            _ => String::new(),
                        };
                        self.lines
                            .push(format!("\t{}: (self: {root}{extra}){ret},", func.name));
                    }
                    _ => {}
                }
            }
            self.lines.push("}".into());
            self.lines.push(String::new());
        }
    }

    fn emit_struct_ctor(&mut self, root: &str) {
        self.lines.push(format!("const function {root}()"));
        self.lines.push("\treturn {".into());
        for item in &self.program.items {
            if let Item::Decl(decl) = item {
                if decl.owner.as_deref() == Some(root) {
                    let value = self.field_literal(decl);
                    self.lines.push(format!("\t\t{} = {value},", decl.name));
                }
            }
        }
        self.lines.push("\t}".into());
        self.lines.push("end".into());
        self.lines.push(String::new());
    }

    fn field_literal(&self, decl: &Decl) -> String {
        if let Some(Expr::InitList { fields }) = &decl.value {
            let inner = fields
                .iter()
                .map(|(n, v)| format!("{} = {}", n, self.emit_expr(v)))
                .collect::<Vec<_>>()
                .join(", ");
            return format!("{{ {inner} }}");
        }
        if let Some(ty) = &decl.value_type {
            if self.nested_types.contains(ty) || self.class_owners.contains(ty) {
                let nested: Vec<_> = self
                    .program
                    .items
                    .iter()
                    .filter_map(|item| match item {
                        Item::Decl(d) if d.owner.as_deref() == Some(ty.as_str()) => Some(d),
                        _ => None,
                    })
                    .collect();
                if !nested.is_empty() {
                    let inner = nested
                        .iter()
                        .map(|d| format!("{} = {}", d.name, self.field_literal(d)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    return format!("{{ {inner} }}");
                }
            }
            if is_array_type(ty) {
                return "{}".into();
            }
        }
        decl.value
            .as_ref()
            .map(|v| self.emit_expr(v))
            .unwrap_or_else(|| "nil".into())
    }

    fn emit_function(&mut self, func: &Function) {
        self.self_owner = func.owner.clone();
        self.local_names.clear();
        for param in &func.params {
            self.local_names.insert(param.name.clone());
        }
        let params = self.param_list(func);
        let ret = return_ann(func);
        if let Some(owner) = &func.owner {
            self.lines
                .push(format!("function {owner}:{}({params}){ret}", func.name));
        } else {
            self.lines
                .push(format!("const function {}({params}){ret}", func.name));
        }
        if let Some(target) = &func.target {
            if self.ctx.script_kind.is_none() {
                let check = if target == "client" {
                    "if game:GetService(\"RunService\"):IsServer() then\n\t\treturn\n\tend"
                } else {
                    "if not game:GetService(\"RunService\"):IsServer() then\n\t\treturn\n\tend"
                };
                self.lines.push(format!("\t{check}"));
            }
        }
        let synth_janitor = stmt_has_cleanup_list(&func.body)
            && !self.local_names.contains("janitor")
            && !(self.self_owner.is_some() && self.class_fields.contains("janitor"))
            && !body_declares_janitor(&func.body);
        if synth_janitor {
            self.local_names.insert("__janitor".into());
            self.lines
                .push("\tlocal __janitor = Janitor.new()".into());
        }
        let body = func.body.clone();
        for stmt in &body {
            let lines = self.emit_stmt(stmt, 1);
            self.lines.extend(lines);
        }
        if synth_janitor && func.name == "init" {
            self.lines.push("\tgame:BindToClose(function()".into());
            self.lines.push("\t\t__janitor:Cleanup()".into());
            self.lines.push("\t\t__janitor:Destroy()".into());
            self.lines.push("\tend)".into());
        }
        self.lines.push("end".into());
        self.lines.push(String::new());
        self.self_owner = None;
        self.local_names.clear();
    }

    fn param_list(&self, func: &Function) -> String {
        func.params
            .iter()
            .map(|p| match luau_type(p.value_type.as_deref()) {
                Some(ty) => format!("{}: {ty}", p.name),
                None => p.name.clone(),
            })
            .collect::<Vec<_>>()
            .join(", ")
    }

    fn emit_decl(&mut self, decl: &Decl, indent: usize) -> Vec<String> {
        if indent > 0 {
            self.local_names.insert(decl.name.clone());
        }
        if decl.is_observable {
            self.observables.insert(decl.name.clone());
        }
        let prefix = "\t".repeat(indent);
        let created = decl.value.as_ref();
        let type_ann = inferred_type(decl);
        let typed = type_ann
            .as_ref()
            .map(|t| format!(": {t}"))
            .unwrap_or_default();

        if let Some(owner) = &decl.owner {
            if indent == 0 {
                if decl.is_observable {
                    let class_name = observable_class(decl.value_type.as_deref().unwrap_or("int"));
                    let mut out = vec![format!(
                        "{prefix}{owner}.{} = Instance.new(\"{class_name}\")",
                        decl.name
                    )];
                    if let Some(value) = &decl.value {
                        out.push(format!(
                            "{prefix}__clpp_obs[{owner}.{}] = true",
                            decl.name
                        ));
                        out.push(format!(
                            "{prefix}{owner}.{}.Value = {} -- initial value; not a Changed event",
                            decl.name,
                            self.emit_expr(value)
                        ));
                        out.push(format!("{prefix}__clpp_obs[{owner}.{}] = nil", decl.name));
                    } else {
                        out.push(format!(
                            "{prefix}__clpp_obs[{owner}.{}] = true",
                            decl.name
                        ));
                    }
                    return out;
                }
                if decl.value_type.as_deref().is_some_and(is_signal_type) && decl.value.is_none() {
                    return vec![format!("{prefix}{owner}.{} = __signal()", decl.name)];
                }
                if let Some(Expr::New { class_name, args }) = created {
                    if is_instance_type(class_name) && !is_library_type(class_name) {
                        let mut out = vec![format!(
                            "{prefix}{owner}.{} = Instance.new(\"{class_name}\")",
                            decl.name
                        )];
                        if let Some(parent) = args.first() {
                            out.push(format!(
                                "{prefix}{owner}.{}.Parent = {}",
                                decl.name,
                                self.emit_expr(parent)
                            ));
                        }
                        return out;
                    }
                    if is_datatype(class_name) || is_library_type(class_name) {
                        let args = args.iter().map(|a| self.emit_expr(a)).collect::<Vec<_>>().join(", ");
                        return vec![format!(
                            "{prefix}{owner}.{} = {class_name}.new({args})",
                            decl.name
                        )];
                    }
                }
                let value = decl
                    .value
                    .as_ref()
                    .map(|v| self.emit_expr(v))
                    .unwrap_or_else(|| "nil".into());
                return vec![format!("{prefix}{owner}.{} = {value}", decl.name)];
            }
        }

        let kind = if decl.is_const { "const" } else { "local" };
        if decl.is_observable {
            let class_name = observable_class(decl.value_type.as_deref().unwrap_or("int"));
            let mut out = vec![format!(
                "{prefix}{kind} {}: {class_name} = Instance.new(\"{class_name}\")",
                decl.name
            )];
            if let Some(value) = &decl.value {
                out.push(format!("{prefix}__clpp_obs[{}] = true", decl.name));
                out.push(format!(
                    "{prefix}{}.Value = {} -- initial value; not a Changed event",
                    decl.name,
                    self.emit_expr(value)
                ));
                out.push(format!("{prefix}__clpp_obs[{}] = nil", decl.name));
            } else {
                out.push(format!("{prefix}__clpp_obs[{}] = true", decl.name));
            }
            return out;
        }
        if decl.value_type.as_deref().is_some_and(is_signal_type) && decl.value.is_none() {
            return vec![format!(
                "{prefix}{kind} {} = __signal()",
                decl.name
            )];
        }
        if let Some(Expr::New { class_name, args }) = created {
            if is_instance_type(class_name) && !is_library_type(class_name) {
                let mut out = vec![format!(
                    "{prefix}{kind} {}{typed} = Instance.new(\"{class_name}\")",
                    decl.name
                )];
                if let Some(parent) = args.first() {
                    out.push(format!(
                        "{prefix}{}.Parent = {}",
                        decl.name,
                        self.emit_expr(parent)
                    ));
                }
                if class_name.ends_with("Value") {
                    self.fresh_values.insert(decl.name.clone());
                }
                return out;
            }
            if is_datatype(class_name) || is_library_type(class_name) {
                let args = args.iter().map(|a| self.emit_expr(a)).collect::<Vec<_>>().join(", ");
                return vec![format!(
                    "{prefix}{kind} {}{typed} = {class_name}.new({args})",
                    decl.name
                )];
            }
        }
        if decl.value.is_none() {
            if let Some(ty) = &decl.value_type {
                if self.class_owners.contains(ty) {
                    return vec![format!("{prefix}{kind} {}{typed} = {ty}", decl.name)];
                }
            }
        }
        let value = decl
            .value
            .as_ref()
            .map(|v| self.emit_expr(v))
            .unwrap_or_else(|| "nil".into());
        vec![format!("{prefix}{kind} {}{typed} = {value}", decl.name)]
    }

    fn emit_stmt(&mut self, stmt: &Stmt, indent: usize) -> Vec<String> {
        let prefix = "\t".repeat(indent);
        match stmt {
            Stmt::Decl(decl) => self.emit_decl(decl, indent),
            Stmt::Destructure { names, value } => self.emit_destructure(names, value, indent),
            Stmt::Expr(expr) => {
                if let Expr::Cast { value_type, .. } = expr {
                    if value_type == "void" {
                        return Vec::new();
                    }
                }
                if let Expr::Binary { op, .. } = expr {
                    if op == "<<" {
                        if let Some(printed) = self.emit_cout(expr) {
                            return printed.into_iter().map(|l| format!("{prefix}{l}")).collect();
                        }
                    }
                }
                if let Expr::Assign { left, right, .. } = expr {
                    if let Expr::Ident(name) = left.as_ref() {
                        if self.observables.contains(name) {
                            let rhs = self.emit_expr(right);
                            return vec![
                                format!("{prefix}{}.Value = {rhs}", self.emit_self_ident(name)),
                                format!("{prefix}__clpp_obs[{}] = nil", self.emit_self_ident(name)),
                            ];
                        }
                    }
                    if let Expr::Member { object, name, .. } = left.as_ref() {
                        if name == "Value" {
                            if let Expr::Ident(id) = object.as_ref() {
                                if self.fresh_values.remove(id) {
                                    return vec![format!(
                                        "{prefix}{}.Value = {} -- initial value; not a Changed event",
                                        self.emit_object(object),
                                        self.emit_expr(right)
                                    )];
                                }
                            }
                        }
                    }
                    if let Expr::New { class_name, args } = right.as_ref() {
                        if is_instance_type(class_name) && !is_library_type(class_name) {
                            let left_s = self.emit_expr(left);
                            let mut out = vec![format!(
                                "{prefix}{left_s} = Instance.new(\"{class_name}\")"
                            )];
                            if let Some(parent) = args.first() {
                                out.push(format!(
                                    "{prefix}{left_s}.Parent = {}",
                                    self.emit_expr(parent)
                                ));
                            }
                            if class_name.ends_with("Value") {
                                if let Expr::Ident(name) = left.as_ref() {
                                    self.fresh_values.insert(name.clone());
                                }
                            }
                            return out;
                        }
                    }
                }
                vec![format!("{prefix}{}", self.emit_expr(expr))]
            }
            Stmt::Return(None) => vec![format!("{prefix}return")],
            Stmt::Return(Some(value)) => vec![format!("{prefix}return {}", self.emit_expr(value))],
            Stmt::Break => vec![format!("{prefix}break")],
            Stmt::If {
                test,
                consequent,
                alternate,
            } => self.emit_if(test, consequent, alternate, indent),
            Stmt::Guard { test, body } => {
                let mut out = vec![format!(
                    "{prefix}if not ({}) then",
                    self.emit_expr(test)
                )];
                for s in body {
                    out.extend(self.emit_stmt(s, indent + 1));
                }
                out.push(format!("{prefix}end"));
                out
            }
            Stmt::While { test, body } => {
                let mut out = vec![format!("{prefix}while {} do", self.emit_expr(test))];
                for s in body {
                    out.extend(self.emit_stmt(s, indent + 1));
                }
                out.push(format!("{prefix}end"));
                out
            }
            Stmt::ForEach { name, iter, body } => {
                self.local_names.insert(name.clone());
                let mut out = vec![format!(
                    "{prefix}for _, {name} in {} do",
                    self.emit_expr(iter)
                )];
                for s in body {
                    out.extend(self.emit_stmt(s, indent + 1));
                }
                out.push(format!("{prefix}end"));
                out
            }
            Stmt::CFor {
                init,
                test,
                incr,
                body,
            } => {
                let mut out = Vec::new();
                if let Some(init_stmt) = init {
                    out.extend(self.emit_stmt(init_stmt, indent));
                }
                let cond = test
                    .as_ref()
                    .map(|e| self.emit_expr(e))
                    .unwrap_or_else(|| "true".into());
                out.push(format!("{prefix}while {cond} do"));
                for s in body {
                    out.extend(self.emit_stmt(s, indent + 1));
                }
                if let Some(step) = incr {
                    out.push(format!("{prefix}\t{}", self.emit_expr(step)));
                }
                out.push(format!("{prefix}end"));
                out
            }
            Stmt::Switch {
                discriminant,
                cases,
            } => self.emit_switch(discriminant, cases, indent),
            Stmt::Match {
                discriminant,
                arms,
            } => self.emit_match(discriminant, arms, indent),
            Stmt::Spawn { body, parallel } => {
                let mut out = vec![format!("{prefix}task.spawn(function()")];
                if *parallel {
                    out.push(format!("{prefix}\ttask.desynchronize()"));
                }
                for s in body {
                    out.extend(self.emit_stmt(s, indent + 1));
                }
                if *parallel {
                    out.push(format!("{prefix}\ttask.synchronize()"));
                }
                out.push(format!("{prefix}end)"));
                out
            }
            Stmt::Block(body) => {
                let mut out = Vec::new();
                for s in body {
                    out.extend(self.emit_stmt(s, indent));
                }
                out
            }
        }
    }

    fn emit_if(
        &mut self,
        test: &Expr,
        consequent: &[Stmt],
        alternate: &Option<Vec<Stmt>>,
        indent: usize,
    ) -> Vec<String> {
        let prefix = "\t".repeat(indent);
        let mut out = vec![format!("{prefix}if {} then", self.emit_expr(test))];
        for s in consequent {
            out.extend(self.emit_stmt(s, indent + 1));
        }
        self.emit_else_chain(alternate, indent, &mut out);
        out.push(format!("{prefix}end"));
        out
    }

    fn emit_else_chain(
        &mut self,
        alternate: &Option<Vec<Stmt>>,
        indent: usize,
        out: &mut Vec<String>,
    ) {
        let Some(alt) = alternate else {
            return;
        };
        let prefix = "\t".repeat(indent);
        if alt.len() == 1 {
            if let Stmt::If {
                test,
                consequent,
                alternate,
            } = &alt[0]
            {
                out.push(format!("{prefix}elseif {} then", self.emit_expr(test)));
                for s in consequent {
                    out.extend(self.emit_stmt(s, indent + 1));
                }
                self.emit_else_chain(alternate, indent, out);
                return;
            }
        }
        out.push(format!("{prefix}else"));
        for s in alt {
            out.extend(self.emit_stmt(s, indent + 1));
        }
    }

    fn emit_destructure(&mut self, names: &[String], value: &Expr, indent: usize) -> Vec<String> {
        for name in names {
            self.local_names.insert(name.clone());
        }
        let prefix = "\t".repeat(indent);
        vec![format!(
            "{prefix}local {} = {}",
            names.join(", "),
            self.emit_expr(value)
        )]
    }

    fn emit_match(&mut self, disc: &Expr, arms: &[MatchArm], indent: usize) -> Vec<String> {
        self.match_id += 1;
        let id = format!("__match{}", self.match_id);
        let prefix = "\t".repeat(indent);
        let inner = "\t".repeat(indent + 1);
        let mut out = vec![format!("{prefix}do")];
        out.push(format!("{inner}local {id} = {}", self.emit_expr(disc)));
        let mut first = true;
        for arm in arms {
            if let Some(class_name) = &arm.class_name {
                let test = if is_instance_type(class_name) {
                    format!("{id}:IsA(\"{class_name}\")")
                } else {
                    let mapped = luau_type(Some(class_name.as_str())).unwrap_or_else(|| class_name.clone());
                    format!("typeof({id}) == \"{mapped}\"")
                };
                let kw = if first { "if" } else { "elseif" };
                first = false;
                out.push(format!("{inner}{kw} {test} then"));
                if let Some(binding) = &arm.binding {
                    self.local_names.insert(binding.clone());
                    out.push(format!("{inner}\tlocal {binding} = {id}"));
                }
            } else {
                out.push(format!("{inner}else"));
            }
            for s in &arm.body {
                out.extend(self.emit_stmt(s, indent + 2));
            }
        }
        if arms.iter().any(|a| a.class_name.is_some()) {
            out.push(format!("{inner}end"));
        }
        out.push(format!("{prefix}end"));
        out
    }

    fn emit_switch(&mut self, disc: &Expr, cases: &[SwitchCase], indent: usize) -> Vec<String> {
        self.switch_id += 1;
        let id = format!("__switch{}", self.switch_id);
        let prefix = "\t".repeat(indent);
        let inner_p = "\t".repeat(indent + 1);
        let simple = matches!(
            disc,
            Expr::Ident(_) | Expr::Number(_) | Expr::String(_) | Expr::Bool(_)
        );
        let subject = if simple {
            self.emit_expr(disc)
        } else {
            id.clone()
        };
        let mut out = vec![format!("{prefix}repeat")];
        if !simple {
            out.push(format!("{inner_p}local {id} = {}", self.emit_expr(disc)));
        }
        let regular: Vec<_> = cases.iter().filter(|c| !c.is_default).collect();
        let fallback = cases.iter().find(|c| c.is_default);
        for (i, case) in regular.iter().enumerate() {
            let tests = case
                .values
                .iter()
                .map(|v| format!("{subject} == {}", self.emit_expr(v)))
                .collect::<Vec<_>>()
                .join(" or ");
            let kw = if i == 0 { "if" } else { "elseif" };
            out.push(format!("{inner_p}{kw} {tests} then"));
            for s in &case.body {
                out.extend(self.emit_stmt(s, indent + 2));
            }
        }
        if let Some(fb) = fallback {
            if regular.is_empty() {
                for s in &fb.body {
                    out.extend(self.emit_stmt(s, indent + 1));
                }
            } else {
                out.push(format!("{inner_p}else"));
                for s in &fb.body {
                    out.extend(self.emit_stmt(s, indent + 2));
                }
            }
        }
        if !regular.is_empty() {
            out.push(format!("{inner_p}end"));
        }
        out.push(format!("{prefix}until true"));
        out
    }

    fn emit_expr(&self, expr: &Expr) -> String {
        match expr {
            Expr::Null => "nil".into(),
            Expr::Bool(v) => if *v { "true" } else { "false" }.into(),
            Expr::Number(n) => n.clone(),
            Expr::String(s) => format!("\"{}\"", escape_lua_string(s)),
            Expr::Interp { parts } => emit_interp(self, parts),
            Expr::Ident(name) => {
                let base = self.emit_self_ident(name);
                if self.observables.contains(name) {
                    format!("{base}.Value")
                } else {
                    base
                }
            }
            Expr::Tuple(values) => values
                .iter()
                .map(|v| self.emit_expr(v))
                .collect::<Vec<_>>()
                .join(", "),
            Expr::This { .. } => "self".into(),
            Expr::AtField { name, .. } => format!("self.{name}"),
            Expr::Await { argument } => format!("__await({})", self.emit_expr(argument)),
            Expr::InitList { fields } => {
                if fields.is_empty() {
                    "{}".into()
                } else {
                    let inner = fields
                        .iter()
                        .map(|(n, v)| format!("{n} = {}", self.emit_expr(v)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{{ {inner} }}")
                }
            }
            Expr::ArrayLit { elements } => {
                if elements.is_empty() {
                    "{}".into()
                } else {
                    let inner = elements
                        .iter()
                        .map(|v| self.emit_expr(v))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{{ {inner} }}")
                }
            }
            Expr::DictLit { pairs } => {
                if pairs.is_empty() {
                    "{}".into()
                } else {
                    let inner = pairs
                        .iter()
                        .map(|(k, v)| match k {
                            Expr::String(s) if is_lua_ident(s) => {
                                format!("{s} = {}", self.emit_expr(v))
                            }
                            _ => format!("[{}] = {}", self.emit_expr(k), self.emit_expr(v)),
                        })
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{{ {inner} }}")
                }
            }
            Expr::Update { op, target } => {
                let dest = self.emit_expr(target);
                if op == "++" {
                    format!("{dest} += 1")
                } else {
                    format!("{dest} -= 1")
                }
            }
            Expr::Cast { argument, .. } => self.emit_expr(argument),
            Expr::Lambda { params, body } => {
                let params = params
                    .iter()
                    .map(|p| match luau_type(p.value_type.as_deref()) {
                        Some(ty) => format!("{}: {ty}", p.name),
                        None => p.name.clone(),
                    })
                    .collect::<Vec<_>>()
                    .join(", ");
                let mut inner = Vec::new();
                let mut clone = Emitter {
                    program: self.program,
                    ctx: self.ctx,
                    lines: Vec::new(),
                    class_owners: self.class_owners.clone(),
                    class_fields: self.class_fields.clone(),
                    class_methods: self.class_methods.clone(),
                    nested_types: self.nested_types.clone(),
                    local_names: self.local_names.clone(),
                    observables: self.observables.clone(),
                    needs_obs_table: self.needs_obs_table,
                    fresh_values: self.fresh_values.clone(),
                    self_owner: self.self_owner.clone(),
                    switch_id: self.switch_id,
                    match_id: self.match_id,
                };
                for p in params.split(", ") {
                    if let Some(name) = p.split(':').next() {
                        clone.local_names.insert(name.trim().to_string());
                    }
                }
                for stmt in body {
                    inner.extend(clone.emit_stmt(stmt, 1));
                }
                if inner.is_empty() {
                    format!("function({params})\nend")
                } else {
                    format!("function({params})\n{}\nend", inner.join("\n"))
                }
            }
            Expr::Unary { op, argument } => {
                let inner = self.emit_expr(argument);
                if op == "!" {
                    format!("not {inner}")
                } else {
                    format!("-{inner}")
                }
            }
            Expr::GetService { service } => format!("game:GetService(\"{service}\")"),
            Expr::New { class_name, args } => {
                if is_library_type(class_name) || !is_instance_type(class_name) {
                    let args = args.iter().map(|a| self.emit_expr(a)).collect::<Vec<_>>().join(", ");
                    format!("{class_name}.new({args})")
                } else {
                    format!("Instance.new(\"{class_name}\")")
                }
            }
            Expr::Member { object, name, .. } => {
                format!("{}.{name}", self.emit_object(object))
            }
            Expr::Call {
                object,
                name,
                args,
                access,
            } => self.emit_call(object.as_deref(), name, args, access),
            Expr::Assign { op, left, right, .. } => {
                format!("{} {op} {}", self.emit_expr(left), self.emit_expr(right))
            }
            Expr::Binary { op, left, right } => {
                if op == "<<" {
                    if let Some(printed) = self.emit_cout(expr) {
                        return printed.join("; ");
                    }
                }
                if op == ".:" {
                    return format!("{} .. {}", self.emit_expr(left), self.emit_expr(right));
                }
                let mapped = match op.as_str() {
                    "!=" => "~=",
                    "&&" => "and",
                    "||" => "or",
                    other => other,
                };
                format!("{} {mapped} {}", self.emit_expr(left), self.emit_expr(right))
            }
        }
    }

    fn emit_call(
        &self,
        object: Option<&Expr>,
        name: &str,
        args: &[Expr],
        access: &str,
    ) -> String {
        if object.is_none() && name == "string_concat" {
            let parts: Vec<_> = args.iter().map(|a| self.emit_expr(a)).collect();
            return match parts.len() {
                0 => "\"\"".into(),
                1 => parts[0].clone(),
                _ => format!("({})", parts.join(" .. ")),
            };
        }
        if object.is_none() && (name == "to_string" || name == "tostring") {
            let inner = args
                .first()
                .map(|a| self.emit_expr(a))
                .unwrap_or_else(|| "nil".into());
            return format!("tostring({inner})");
        }
        if object.is_none() && (name == "to_number" || name == "tonumber") {
            let inner = args
                .iter()
                .map(|a| self.emit_expr(a))
                .collect::<Vec<_>>()
                .join(", ");
            return format!("tonumber({inner})");
        }
        if object.is_none() && name == "to_bool" {
            let inner = args
                .first()
                .map(|a| self.emit_expr(a))
                .unwrap_or_else(|| "nil".into());
            return format!("not not ({inner})");
        }
        let args_s = args
            .iter()
            .map(|a| self.emit_method_arg(a))
            .collect::<Vec<_>>()
            .join(", ");
        let name = map_builtin(name);
        if object.is_none() {
            if is_datatype(name) {
                return format!("{name}.new({args_s})");
            }
            if self.self_owner.is_some()
                && self.class_methods.contains(name)
                && !self.local_names.contains(name)
            {
                return format!("self:{name}({args_s})");
            }
            return format!("{name}({args_s})");
        }
        let obj_expr = object.unwrap();
        let obj = self.emit_object(obj_expr);
        if name == "OnChange" && self.observable_instance(obj_expr).is_some()
            || name == "Connect" && self.is_observable_changed(obj_expr)
        {
            let target = if name == "OnChange" {
                format!("{obj}.Changed")
            } else {
                obj.clone()
            };
            let conn = self.emit_obs_connect(&target, obj_expr, &args_s);
            if access == "~>" {
                return format!("{}:Add({conn}, \"Disconnect\")", self.janitor_expr());
            }
            return self.protect_call(conn, access);
        }
        if access == "~>" {
            return format!(
                "{}:Add({obj}:{name}({args_s}), \"Disconnect\")",
                self.janitor_expr()
            );
        }
        if self.class_methods.contains(name) && !self.local_names.contains(name) {
            return self.protect_call(format!("{obj}:{name}({args_s})"), access);
        }
        if let Expr::Ident(n) = obj_expr {
            if n == "cout" {
                let mapped = match name {
                    "warn" => "warn",
                    "error" | "report" => "error",
                    _ => "print",
                };
                return format!("{mapped}({args_s})");
            }
            if is_datatype(n) || is_luau_lib(n) {
                return self.protect_call(format!("{obj}.{name}({args_s})"), access);
            }
        }
        let colon = self.call_uses_colon(obj_expr, name, access);
        let call = format!(
            "{obj}{}{name}({args_s})",
            if colon { ":" } else { "." }
        );
        self.protect_call(call, access)
    }

    fn call_uses_colon(&self, obj_expr: &Expr, name: &str, access: &str) -> bool {
        if let Expr::Ident(n) = obj_expr {
            if is_datatype(n) || is_luau_lib(n) {
                return false;
            }
            if is_library_type(n) && !is_method(name) {
                return false;
            }
        }
        match access {
            "::" | ":" => true,
            "." => is_method(name) || self.class_methods.contains(name),
            _ => false,
        }
    }

    fn protect_call(&self, call: String, access: &str) -> String {
        if access != ":" {
            return call;
        }
        format!(
            "(function() local _ok, _r = pcall(function() return {call} end); return if _ok then _r else nil end)()"
        )
    }

    fn emit_object(&self, expr: &Expr) -> String {
        match expr {
            Expr::Ident(name) => self.emit_self_ident(name),
            Expr::This { .. } => "self".into(),
            Expr::AtField { name, .. } => format!("self.{name}"),
            Expr::Member { object, name, .. } => {
                format!("{}.{name}", self.emit_object(object))
            }
            other => self.emit_expr(other),
        }
    }

    fn janitor_expr(&self) -> String {
        if self.local_names.contains("janitor") {
            "janitor".into()
        } else if self.local_names.contains("__janitor") {
            "__janitor".into()
        } else if self.self_owner.is_some() && self.class_fields.contains("janitor") {
            "self.janitor".into()
        } else {
            "__janitor".into()
        }
    }

    fn observable_instance(&self, expr: &Expr) -> Option<String> {
        match expr {
            Expr::Ident(name) if self.observables.contains(name) => {
                Some(self.emit_self_ident(name))
            }
            Expr::Member { object, name, .. } if name == "Changed" => {
                self.observable_instance(object)
            }
            _ => None,
        }
    }

    fn is_observable_changed(&self, expr: &Expr) -> bool {
        self.observable_instance(expr).is_some()
    }

    fn emit_obs_connect(&self, signal: &str, object: &Expr, handler: &str) -> String {
        let inst = self
            .observable_instance(object)
            .unwrap_or_else(|| "nil".into());
        format!(
            "(function(__fn) return {signal}:Connect(function(...)\n\t\tif __clpp_obs[{inst}] then\n\t\t\treturn\n\t\tend\n\t\treturn __fn(...)\n\tend) end)({handler})"
        )
    }

    fn emit_method_arg(&self, arg: &Expr) -> String {
        if let Expr::Ident(name) = arg {
            if self.self_owner.is_some()
                && self.class_methods.contains(name)
                && !self.local_names.contains(name)
            {
                return format!("function(...) self:{name}(...) end");
            }
            return self.emit_self_ident(name);
        }
        self.emit_expr(arg)
    }

    fn emit_self_ident(&self, name: &str) -> String {
        if name == "this" {
            return "self".into();
        }
        if self.self_owner.is_none() || self.local_names.contains(name) || is_bare_global(name) {
            return name.to_string();
        }
        if self.class_fields.contains(name) {
            return format!("self.{name}");
        }
        name.to_string()
    }

    fn emit_cout(&self, expr: &Expr) -> Option<Vec<String>> {
        let mut parts = Vec::new();
        let mut current = expr;
        while let Expr::Binary { op, left, right } = current {
            if op != "<<" {
                break;
            }
            parts.insert(0, right.as_ref());
            current = left.as_ref();
        }
        let fn_name = stream_print(current)?;
        let mut lines = Vec::new();
        let mut current_args: Vec<String> = Vec::new();
        for part in parts {
            if matches!(part, Expr::Ident(n) if n == "endl") {
                if !current_args.is_empty() {
                    lines.push(format!("{fn_name}({})", current_args.join(", ")));
                    current_args.clear();
                }
            } else {
                current_args.push(self.emit_expr(part));
            }
        }
        if !current_args.is_empty() {
            lines.push(format!("{fn_name}({})", current_args.join(", ")));
        }
        if lines.is_empty() {
            lines.push(format!("{fn_name}()"));
        }
        Some(lines)
    }
}

fn program_uses_cleanup(program: &Program) -> bool {
    program.items.iter().any(|item| match item {
        Item::Function(f) | Item::Proto(f) => stmt_has_cleanup_list(&f.body),
        Item::Decl(d) => d.value.as_ref().is_some_and(expr_has_cleanup),
        Item::Destructure { value, .. } => expr_has_cleanup(value),
    })
}

fn program_uses_await(program: &Program) -> bool {
    program.items.iter().any(|item| match item {
        Item::Function(f) | Item::Proto(f) => f.is_async || stmt_has_await_list(&f.body),
        Item::Decl(d) => d.value.as_ref().is_some_and(expr_has_await),
        Item::Destructure { value, .. } => expr_has_await(value),
    })
}

fn program_uses_signal(program: &Program) -> bool {
    program.items.iter().any(|item| match item {
        Item::Decl(d) => d.value_type.as_deref().is_some_and(is_signal_type),
        Item::Function(f) | Item::Proto(f) => stmt_has_signal_decl(&f.body),
        _ => false,
    })
}

fn stmt_has_signal_decl(body: &[Stmt]) -> bool {
    body.iter().any(|stmt| match stmt {
        Stmt::Decl(d) => d.value_type.as_deref().is_some_and(is_signal_type),
        Stmt::If {
            consequent,
            alternate,
            ..
        } => {
            stmt_has_signal_decl(consequent)
                || alternate.as_ref().is_some_and(|a| stmt_has_signal_decl(a))
        }
        Stmt::While { body, .. }
        | Stmt::ForEach { body, .. }
        | Stmt::CFor { body, .. }
        | Stmt::Spawn { body, .. }
        | Stmt::Block(body)
        | Stmt::Guard { body, .. } => stmt_has_signal_decl(body),
        Stmt::Switch { cases, .. } => cases.iter().any(|c| stmt_has_signal_decl(&c.body)),
        Stmt::Match { arms, .. } => arms.iter().any(|a| stmt_has_signal_decl(&a.body)),
        _ => false,
    })
}

fn body_declares_janitor(body: &[Stmt]) -> bool {
    body.iter().any(|stmt| match stmt {
        Stmt::Decl(d) if d.name == "janitor" => true,
        Stmt::Block(b) => body_declares_janitor(b),
        _ => false,
    })
}

fn stmt_has_cleanup_list(body: &[Stmt]) -> bool {
    body.iter().any(stmt_has_cleanup)
}

fn stmt_has_cleanup(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Expr(e) | Stmt::Return(Some(e)) => expr_has_cleanup(e),
        Stmt::Decl(d) => d.value.as_ref().is_some_and(expr_has_cleanup),
        Stmt::Destructure { value, .. } => expr_has_cleanup(value),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => {
            expr_has_cleanup(test)
                || stmt_has_cleanup_list(consequent)
                || alternate.as_ref().is_some_and(|a| stmt_has_cleanup_list(a))
        }
        Stmt::Guard { test, body } => expr_has_cleanup(test) || stmt_has_cleanup_list(body),
        Stmt::While { test, body } => expr_has_cleanup(test) || stmt_has_cleanup_list(body),
        Stmt::ForEach { iter, body, .. } => expr_has_cleanup(iter) || stmt_has_cleanup_list(body),
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => {
            init.as_ref().is_some_and(|s| stmt_has_cleanup(s))
                || test.as_ref().is_some_and(expr_has_cleanup)
                || incr.as_ref().is_some_and(expr_has_cleanup)
                || stmt_has_cleanup_list(body)
        }
        Stmt::Switch {
            discriminant,
            cases,
        } => {
            expr_has_cleanup(discriminant) || cases.iter().any(|c| stmt_has_cleanup_list(&c.body))
        }
        Stmt::Match {
            discriminant,
            arms,
        } => expr_has_cleanup(discriminant) || arms.iter().any(|a| stmt_has_cleanup_list(&a.body)),
        Stmt::Spawn { body, .. } | Stmt::Block(body) => stmt_has_cleanup_list(body),
        _ => false,
    }
}

fn expr_has_cleanup(expr: &Expr) -> bool {
    match expr {
        Expr::Call {
            access,
            args,
            object,
            ..
        } => {
            access == "~>"
                || args.iter().any(expr_has_cleanup)
                || object.as_ref().is_some_and(|o| expr_has_cleanup(o))
        }
        Expr::Unary { argument, .. }
        | Expr::Cast { argument, .. }
        | Expr::Await { argument }
        | Expr::Update { target: argument, .. } => expr_has_cleanup(argument),
        Expr::Binary { left, right, .. } | Expr::Assign { left, right, .. } => {
            expr_has_cleanup(left) || expr_has_cleanup(right)
        }
        Expr::Member { object, .. } => expr_has_cleanup(object),
        Expr::New { args, .. } => args.iter().any(expr_has_cleanup),
        Expr::Lambda { body, .. } => stmt_has_cleanup_list(body),
        Expr::InitList { fields } => fields.iter().any(|(_, v)| expr_has_cleanup(v)),
        Expr::ArrayLit { elements } => elements.iter().any(expr_has_cleanup),
        Expr::DictLit { pairs } => pairs
            .iter()
            .any(|(k, v)| expr_has_cleanup(k) || expr_has_cleanup(v)),
        Expr::Interp { parts } => parts.iter().any(|part| match part {
            crate::ast::InterpPart::Value(expr) => expr_has_cleanup(expr),
            crate::ast::InterpPart::Text(_) => false,
        }),
        _ => false,
    }
}

fn stmt_has_await_list(body: &[Stmt]) -> bool {
    body.iter().any(stmt_has_await)
}

fn stmt_has_await(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Expr(e) | Stmt::Return(Some(e)) => expr_has_await(e),
        Stmt::Decl(d) => d.value.as_ref().is_some_and(expr_has_await),
        Stmt::Destructure { value, .. } => expr_has_await(value),
        Stmt::If {
            test,
            consequent,
            alternate,
        } => {
            expr_has_await(test)
                || stmt_has_await_list(consequent)
                || alternate.as_ref().is_some_and(|a| stmt_has_await_list(a))
        }
        Stmt::Guard { test, body }
        | Stmt::While { test, body } => expr_has_await(test) || stmt_has_await_list(body),
        Stmt::ForEach { iter, body, .. } => expr_has_await(iter) || stmt_has_await_list(body),
        Stmt::CFor {
            init,
            test,
            incr,
            body,
        } => {
            init.as_ref().is_some_and(|s| stmt_has_await(s))
                || test.as_ref().is_some_and(expr_has_await)
                || incr.as_ref().is_some_and(expr_has_await)
                || stmt_has_await_list(body)
        }
        Stmt::Switch {
            discriminant,
            cases,
        } => expr_has_await(discriminant) || cases.iter().any(|c| stmt_has_await_list(&c.body)),
        Stmt::Match {
            discriminant,
            arms,
        } => expr_has_await(discriminant) || arms.iter().any(|a| stmt_has_await_list(&a.body)),
        Stmt::Spawn { body, .. } | Stmt::Block(body) => stmt_has_await_list(body),
        _ => false,
    }
}

fn expr_has_await(expr: &Expr) -> bool {
    match expr {
        Expr::Await { .. } => true,
        Expr::Unary { argument, .. }
        | Expr::Cast { argument, .. }
        | Expr::Update { target: argument, .. } => expr_has_await(argument),
        Expr::Binary { left, right, .. } | Expr::Assign { left, right, .. } => {
            expr_has_await(left) || expr_has_await(right)
        }
        Expr::Member { object, .. } => expr_has_await(object),
        Expr::Call { object, args, .. } => {
            object.as_ref().is_some_and(|o| expr_has_await(o)) || args.iter().any(expr_has_await)
        }
        Expr::New { args, .. } => args.iter().any(expr_has_await),
        Expr::Lambda { body, .. } => stmt_has_await_list(body),
        Expr::InitList { fields } => fields.iter().any(|(_, v)| expr_has_await(v)),
        Expr::ArrayLit { elements } => elements.iter().any(expr_has_await),
        Expr::DictLit { pairs } => pairs
            .iter()
            .any(|(k, v)| expr_has_await(k) || expr_has_await(v)),
        Expr::Interp { parts } => parts.iter().any(|part| match part {
            crate::ast::InterpPart::Value(expr) => expr_has_await(expr),
            crate::ast::InterpPart::Text(_) => false,
        }),
        _ => false,
    }
}

fn emit_interp(emitter: &Emitter, parts: &[crate::ast::InterpPart]) -> String {
    if parts.is_empty() {
        return "\"\"".into();
    }
    let bits: Vec<String> = parts
        .iter()
        .map(|part| match part {
            crate::ast::InterpPart::Text(text) => {
                format!("\"{}\"", escape_lua_string(text))
            }
            crate::ast::InterpPart::Value(expr) => {
                format!("tostring({})", emitter.emit_expr(expr))
            }
        })
        .collect();
    if bits.len() == 1 {
        bits[0].clone()
    } else {
        bits.join(" .. ")
    }
}

fn escape_lua_string(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    for ch in text.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            '"' => out.push_str("\\\""),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            other => out.push(other),
        }
    }
    out
}

fn map_builtin(name: &str) -> &str {
    match name {
        "post" => "print",
        "report" => "error",
        other => other,
    }
}

fn is_lua_ident(s: &str) -> bool {
    let mut chars = s.chars();
    matches!(chars.next(), Some(c) if c.is_ascii_alphabetic() || c == '_')
        && chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
}

fn stream_print(expr: &Expr) -> Option<&'static str> {
    match expr {
        Expr::Ident(n) if n == "cout" => Some("print"),
        Expr::Ident(n) if n == "cerr" => Some("warn"),
        Expr::Member { object, name, .. } => {
            if matches!(object.as_ref(), Expr::Ident(n) if n == "cout") {
                Some(match name.as_str() {
                    "warn" => "warn",
                    "error" => "error",
                    _ => "print",
                })
            } else {
                None
            }
        }
        _ => None,
    }
}

fn inferred_type(decl: &Decl) -> Option<String> {
    match &decl.value {
        Some(Expr::New { class_name, .. }) => Some(class_name.clone()),
        Some(Expr::Call { object: None, name, .. }) if is_datatype(name) => Some(name.clone()),
        Some(Expr::GetService { service }) => Some(service.clone()),
        _ => luau_type(decl.value_type.as_deref()).filter(|t| t != "auto"),
    }
}

fn return_ann(func: &Function) -> String {
    match luau_type(func.return_type.as_deref()) {
        Some(ty) if ty != "()" => format!(": {ty}"),
        _ => String::new(),
    }
}

fn rojo_require(from_file: &str, to_file: &str) -> String {
    let from_dir = Path::new(from_file).parent().unwrap_or_else(|| Path::new("."));
    let to = Path::new(to_file);
    let to_stem = PathBuf::from(to.file_stem().unwrap_or_default());
    let to_dir = to.parent().unwrap_or_else(|| Path::new("."));
    let rel = pathdiff_fallback(from_dir, &to_dir.join(&to_stem));
    let mut expr = String::from("script.Parent");
    for part in rel.split(['/', '\\']) {
        if part.is_empty() || part == "." {
            continue;
        }
        if part == ".." {
            expr.push_str(".Parent");
        } else {
            expr.push('.');
            expr.push_str(part);
        }
    }
    expr
}

fn pathdiff_fallback(from_dir: &Path, to: &Path) -> String {
    let from = from_dir.components().map(|c| c.as_os_str().to_string_lossy().into_owned()).collect::<Vec<_>>();
    let to = to.components().map(|c| c.as_os_str().to_string_lossy().into_owned()).collect::<Vec<_>>();
    let mut i = 0;
    while i < from.len() && i < to.len() && from[i] == to[i] {
        i += 1;
    }
    let mut parts = vec![".."; from.len() - i];
    let rest: Vec<&str> = to[i..].iter().map(String::as_str).collect();
    parts.extend(rest);
    parts.join("/")
}
