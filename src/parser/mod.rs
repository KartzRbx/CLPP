use crate::ast::*;
use crate::error::ClppError;
use crate::support::CompileDiagnostic;
use miette::Result;
use pest::error::ErrorVariant;
use pest::iterators::Pair;
use pest::Parser as PestParser;
use pest_derive::Parser;

#[derive(Parser)]
#[grammar = "src/parser/grammar.pest"]
pub struct ClppParser;

pub fn parse_with_diagnostics(
    source: &str,
    file_name: &str,
) -> Result<(Program, Vec<CompileDiagnostic>)> {
    parse_tree(source, file_name)
}

pub fn parse(source: &str, file_name: &str) -> Result<Program> {
    parse_with_diagnostics(source, file_name).map(|(program, _)| program)
}

/// Parse for the IDE: recover after the first Pest error so later symbols remain.
pub fn parse_for_ide(source: &str, file_name: &str) -> (Program, Vec<CompileDiagnostic>) {
    match parse_tree(source, file_name) {
        Ok((program, diags)) => (program, diags),
        Err(err) => {
            let mut diagnostics = Vec::new();
            let (line, column, message) = if let Some(clpp) = err.downcast_ref::<ClppError>() {
                let (line, column) = clpp.line_col();
                (line, column, clpp.message.clone())
            } else {
                (1, 1, format!("{err:#}"))
            };
            diagnostics.push(CompileDiagnostic {
                message: message.clone(),
                line,
                column,
                severity: "error".into(),
                code: None,
                help: None,
            });
            let patched = patch_incomplete_access(source);
            if patched != source {
                if let Ok((mut program, extra)) = parse_tree(&patched, file_name) {
                    diagnostics.extend(extra);
                    program.file_name = file_name.to_string();
                    return (program, diagnostics);
                }
            }
            let ghosted = inject_ghost(source, line, column);
            if ghosted != source {
                if let Ok((mut program, extra)) = parse_tree(&ghosted, file_name) {
                    diagnostics.extend(extra);
                    program.file_name = file_name.to_string();
                    return (program, diagnostics);
                }
            }
            let recovered = recover_source(source, line);
            if recovered != source {
                if let Ok((mut program, extra)) = parse_tree(&recovered, file_name) {
                    diagnostics.extend(extra);
                    program.file_name = file_name.to_string();
                    return (program, diagnostics);
                }
            }
            (
                Program {
                    items: Vec::new(),
                    file_name: file_name.to_string(),
                },
                diagnostics,
            )
        }
    }
}

fn parse_tree(source: &str, file_name: &str) -> Result<(Program, Vec<CompileDiagnostic>)> {
    let mut pairs = ClppParser::parse(Rule::file, source).map_err(|err| {
        let (line, col) = match err.line_col {
            pest::error::LineColLocation::Pos((l, c)) => (l, c),
            pest::error::LineColLocation::Span((l, c), _) => (l, c),
        };
        ClppError::at_line(source, line, col, format_parse_error(&err))
    })?;
    let file = pairs.next().expect("file pair");
    let mut items = Vec::new();
    let mut diagnostics = Vec::new();
    for pair in file.into_inner() {
        if pair.as_rule() == Rule::item {
            items.extend(parse_item(pair, &mut diagnostics));
        }
    }
    Ok((
        Program {
            items,
            file_name: file_name.to_string(),
        },
        diagnostics,
    ))
}

fn patch_incomplete_access(source: &str) -> String {
    let mut changed = false;
    let lines: Vec<String> = source
        .lines()
        .map(|line| {
            let t = line.trim_end();
            if (t.ends_with('.') && !t.ends_with(".:") && !t.ends_with(".."))
                || t.ends_with("~>")
                || t.ends_with("::")
                || t.ends_with('@')
            {
                changed = true;
                format!("{t}__clpp_complete;")
            } else {
                line.to_string()
            }
        })
        .collect();
    if changed {
        lines.join("\n")
    } else {
        source.to_string()
    }
}

fn inject_ghost(source: &str, err_line: usize, err_col: usize) -> String {
    let mut lines: Vec<String> = source.lines().map(|l| l.to_string()).collect();
    if err_line == 0 || err_line > lines.len() {
        return source.to_string();
    }
    let row = &mut lines[err_line - 1];
    let idx = err_col.saturating_sub(1).min(row.len());
    row.insert_str(idx, "__clpp_complete");
    let trimmed = row.trim_end();
    if !trimmed.ends_with(';') && !trimmed.ends_with('{') && !trimmed.ends_with('}') {
        row.push(';');
    }
    lines.join("\n")
}

fn recover_source(source: &str, err_line: usize) -> String {
    let mut lines: Vec<String> = source.lines().map(|l| l.to_string()).collect();
    if err_line > 0 && err_line <= lines.len() {
        lines[err_line - 1] = String::new();
    }
    let mut text = lines.join("\n");
    let opens = text.matches('{').count();
    let closes = text.matches('}').count();
    for _ in 0..opens.saturating_sub(closes) {
        text.push_str("\n}");
    }
    text
}

fn pair_span(pair: &Pair<Rule>) -> Span {
    let (start_line, start_col) = pair.line_col();
    let (end_line, end_col) = pair.as_span().end_pos().line_col();
    Span::new(start_line, start_col, end_line, end_col)
}

fn format_parse_error(err: &pest::error::Error<Rule>) -> String {
    let raw = err.to_string();
    let expected_semi = match &err.variant {
        ErrorVariant::ParsingError { positives, .. } => positives.iter().any(|rule| {
            let name = format!("{rule:?}");
            name.contains("EOI") || raw.contains("\";\"")
        }),
        _ => false,
    };
    if expected_semi || raw.contains("expected \";\"") || raw.contains("expected \";\"") {
        return "missing ';' at the end of this statement".into();
    }
    if raw.contains("expected ident") {
        return "expected a name after the type, e.g. const int coins = 0;".into();
    }
    if let ErrorVariant::ParsingError { positives, .. } = &err.variant {
        let names: Vec<String> = positives.iter().map(|r| format!("{r:?}")).collect();
        if names.iter().any(|n| n.contains("ident")) {
            return "expected a name after the type, e.g. const int coins = 0;".into();
        }
        if names.iter().any(|n| {
            n.contains("assign_op") || n.contains("postfix") || n.contains("cmp_op")
        }) {
            return "syntax error — missing ';', a bad operator, or an unexpected token".into();
        }
    }
    raw.lines()
        .last()
        .map(|l| l.trim().trim_start_matches('=').trim())
        .filter(|l| !l.is_empty())
        .map(|l| l.to_string())
        .unwrap_or(raw)
}

fn parse_item(pair: Pair<Rule>, diagnostics: &mut Vec<CompileDiagnostic>) -> Vec<Item> {
    let inner = pair.into_inner().next().expect("item inner");
    match inner.as_rule() {
        Rule::struct_decl | Rule::interface_decl => parse_struct(inner, None, diagnostics),
        Rule::enum_decl => vec![parse_enum(inner)],
        Rule::function_item => vec![parse_function(inner)],
        Rule::field_destructure => {
            let (names, value) = parse_field_destructure_parts(inner);
            vec![Item::Destructure { names, value }]
        }
        Rule::destructure => {
            let (names, value) = parse_destructure_parts(inner);
            vec![Item::Destructure { names, value }]
        }
        Rule::var_decl => vec![Item::Decl(parse_var_decl(inner, None))],
        Rule::using_namespace => {
            let line = inner.line_col().0;
            diagnostics.push(CompileDiagnostic {
                message: crate::diag::message(crate::diag::CLPP0301, "`using namespace` is not in CL++"),
                line,
                column: 1,
                severity: "error".into(),
                code: Some("CLPP0301".into()),
                help: Some(crate::diag::CLPP0301.help.into()),
            });
            vec![Item::Unsupported {
                kind: "using namespace".into(),
                line,
                message: "`using namespace` is not in CL++".into(),
            }]
        }
        Rule::using_stmt | Rule::type_alias_decl => vec![parse_using(inner)],
        Rule::import_decl => {
            let line = inner.line_col().0;
            diagnostics.push(CompileDiagnostic {
                message: "use `link`".into(),
                line,
                column: 1,
                severity: "error".into(),
                code: Some("CLPP0801".into()),
                help: Some("write `link \"./path\" as Name;`, `link @clpp…`, or `link @game…`".into()),
            });
            vec![Item::Unsupported {
                kind: "import".into(),
                line,
                message: "use `link`".into(),
            }]
        }
        Rule::link_decl => vec![parse_link(inner)],
        Rule::namespace_item => {
            let line = inner.line_col().0;
            diagnostics.push(CompileDiagnostic {
                message: crate::diag::message(crate::diag::CLPP0301, "`namespace` is not in CL++ — use a struct or a module file"),
                line,
                column: 1,
                severity: "error".into(),
                code: Some("CLPP0301".into()),
                help: Some(crate::diag::CLPP0301.help.into()),
            });
            let mut items = vec![Item::Unsupported {
                kind: "namespace".into(),
                line,
                message: "`namespace` is not in CL++".into(),
            }];
            items.extend(
                inner
                    .into_inner()
                    .filter(|p| p.as_rule() == Rule::item)
                    .flat_map(|p| parse_item(p, diagnostics)),
            );
            items
        }
        Rule::hash_line => {
            let text = inner.as_str().trim();
            if text.starts_with("#include") || text.starts_with("# include") {
                let line = inner.line_col().0;
                diagnostics.push(CompileDiagnostic {
                    message: "use `link`".into(),
                    line,
                    column: 1,
                    severity: "error".into(),
                    code: Some("CLPP0801".into()),
                    help: Some("write `link \"./path\" as Name;`, `link @clpp…`, or `link @game…`".into()),
                });
                vec![Item::Unsupported {
                    kind: "include".into(),
                    line,
                    message: "use `link`".into(),
                }]
            } else {
                Vec::new()
            }
        }
        Rule::skip_decl => {
            let line = inner.line_col().0;
            let kind = if inner.as_str().trim_start().starts_with("enum") {
                "enum"
            } else if inner.as_str().trim_start().starts_with("template") {
                "template"
            } else if inner.as_str().trim_start().starts_with("typedef") {
                "typedef"
            } else {
                "extern"
            };
            diagnostics.push(CompileDiagnostic {
                message: crate::diag::message(crate::diag::CLPP0301, format!("`{kind}` is not in CL++")),
                line,
                column: 1,
                severity: "error".into(),
                code: Some("CLPP0301".into()),
                help: Some(crate::diag::CLPP0301.help.into()),
            });
            vec![Item::Unsupported {
                kind: kind.into(),
                line,
                message: format!("`{kind}` is not in CL++"),
            }]
        }
        _ => Vec::new(),
    }
}

fn parse_link(pair: Pair<Rule>) -> Item {
    let span = pair_span(&pair);
    let mut module = String::new();
    let mut alias = None;
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::link_target => {
                let text = inner.as_str().trim().trim_matches('"').to_string();
                module = text;
            }
            Rule::ident => alias = Some(inner.as_str().to_string()),
            _ => {}
        }
    }
    let name = alias.unwrap_or_else(|| {
        module
            .rsplit(['/', '.'])
            .next()
            .unwrap_or("Module")
            .trim_end_matches(".clh")
            .trim_end_matches(".clpp")
            .trim_end_matches(".clp")
            .to_string()
    });
    Item::Import {
        names: vec![crate::ast::ImportName { name, alias: None }],
        module,
        line: span.start_line,
        span,
    }
}

fn parse_import(pair: Pair<Rule>) -> Item {
    let span = pair_span(&pair);
    let mut names = Vec::new();
    let mut module = String::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::import_binding => names.push(parse_import_binding(inner)),
            Rule::string => module = inner.as_str().trim_matches('"').to_string(),
            _ => {}
        }
    }
    Item::Import {
        names,
        module,
        line: span.start_line,
        span,
    }
}

fn parse_import_binding(pair: Pair<Rule>) -> crate::ast::ImportName {
    let mut idents = pair
        .into_inner()
        .filter(|p| p.as_rule() == Rule::ident)
        .map(|p| p.as_str().to_string())
        .collect::<Vec<_>>();
    let name = idents.remove(0);
    let alias = idents.pop();
    crate::ast::ImportName { name, alias }
}

fn parse_using(pair: Pair<Rule>) -> Item {
    let span = pair_span(&pair);
    let mut name = "Alias".into();
    let mut ty = "any".into();
    let mut type_params = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::ident => name = inner.as_str().to_string(),
            Rule::type_spec => ty = parse_type(inner),
            Rule::type_generic => type_params = parse_generic_args(inner),
            _ => {}
        }
    }
    Item::TypeAlias {
        name,
        ty,
        type_params,
        line: span.start_line,
        span,
        doc: None,
    }
}

fn parse_enum(pair: Pair<Rule>) -> Item {
    let span = pair_span(&pair);
    let mut name = "Enum".into();
    let mut numeric = false;
    let mut variants = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::ident => name = inner.as_str().to_string(),
            Rule::type_spec => numeric = true,
            Rule::enum_variant => {
                let mut vname = String::new();
                let mut val = None;
                for p in inner.into_inner() {
                    match p.as_rule() {
                        Rule::ident => vname = p.as_str().to_string(),
                        Rule::number => val = Some(p.as_str().to_string()),
                        _ => {}
                    }
                }
                if !vname.is_empty() {
                    variants.push((vname, val));
                }
            }
            _ => {}
        }
    }
    Item::Enum {
        name,
        numeric,
        variants,
        line: span.start_line,
        span,
        doc: None,
    }
}

fn parse_field_destructure_parts(pair: Pair<Rule>) -> (Vec<String>, Expr) {
    let mut names = Vec::new();
    let mut value = Expr::Null;
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::ident => names.push(inner.as_str().to_string()),
            Rule::expr => value = parse_expr(inner),
            _ => {}
        }
    }
    (names, value)
}

fn parse_struct(pair: Pair<Rule>, nested_owner: Option<&str>, diagnostics: &mut Vec<CompileDiagnostic>) -> Vec<Item> {
    let span = pair_span(&pair);
    let mut items = Vec::new();
    let mut name = "_anon".to_string();
    let mut parent = None;
    let _ = nested_owner;
    let mut type_params = Vec::new();
    let mut saw_name = false;
    let mut vis = "public".to_string();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::template_head => {
                for p in inner.into_inner() {
                    if p.as_rule() == Rule::template_param {
                        type_params.push(parse_template_param(p));
                    }
                }
            }
            Rule::ident => {
                if !saw_name {
                    name = inner.as_str().to_string();
                    saw_name = true;
                } else {
                    parent = Some(inner.as_str().to_string());
                }
            }
            Rule::type_generic => {
                type_params.extend(
                    parse_generic_args(inner)
                        .into_iter()
                        .map(TypeParam::unbound),
                );
            }
            Rule::struct_member => items.extend(parse_struct_member(inner, &name, &mut vis, diagnostics)),
            _ => {}
        }
    }
    items.insert(
        0,
        Item::Class {
            name: name.clone(),
            parent: parent.clone(),
            type_params,
            line: span.start_line,
            span,
            doc: None,
        },
    );
    items
}

fn parse_struct_member(
    pair: Pair<Rule>,
    owner: &str,
    vis: &mut String,
    diagnostics: &mut Vec<CompileDiagnostic>,
) -> Vec<Item> {
    let inner = pair.into_inner().next().expect("member");
    match inner.as_rule() {
        Rule::access_label => {
            let text = inner.as_str();
            *vis = if text.contains("private") {
                "private".into()
            } else if text.contains("protected") {
                "protected".into()
            } else {
                "public".into()
            };
            Vec::new()
        }
        Rule::nested_struct => {
            let mut nested = inner.into_inner();
            let Some(decl) = nested.next() else {
                return Vec::new();
            };
            let mut items = parse_struct(decl, Some(owner), diagnostics);
            if let Some(instance) = nested.find(|p| p.as_rule() == Rule::ident) {
                let line = instance.line_col().0;
                items.push(Item::Decl(Decl {
                    name: instance.as_str().to_string(),
                    value_type: Some(owner.to_string()),
                    value: None,
                    is_const: false,
                    is_observable: false,
                    owner: Some(owner.to_string()),
                    line,
                    span: Span::point(line, 1),
                    doc: None,
                    visibility: Some(vis.clone()),
                }));
            }
            items
        }
        Rule::ctor_proto => vec![parse_ctor_proto(inner, owner, vis)],
        Rule::method_proto => {
            let mut func = match parse_function(inner) {
                Item::Function(f) | Item::Proto(f) => f,
                _ => return Vec::new(),
            };
            func.owner = Some(owner.to_string());
            func.visibility = Some(vis.clone());
            vec![Item::Proto(func)]
        }
        Rule::field_decl => {
            let mut decl = parse_var_decl(inner, Some(owner));
            decl.visibility = Some(vis.clone());
            vec![Item::Decl(decl)]
        }
        _ => Vec::new(),
    }
}

fn parse_ctor_proto(pair: Pair<Rule>, owner: &str, vis: &str) -> Item {
    let span = pair_span(&pair);
    let line = span.start_line;
    let mut name = owner.to_string();
    let mut params = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::ident => name = inner.as_str().to_string(),
            Rule::param_list => params = parse_params(inner),
            _ => {}
        }
    }
    Item::Proto(Function {
        name,
        owner: Some(owner.to_string()),
        return_type: None,
        params,
        body: Vec::new(),
        is_const: false,
        is_async: false,
        target: None,
        span,
        doc: None,
        is_static: false,
        is_override: false,
        visibility: Some(vis.to_string()),
        type_params: Vec::new(),
        attrs: Vec::new(),
        parent: Some(owner.to_string()),
        line,
    })
}

fn parse_function(pair: Pair<Rule>) -> Item {
    let span = pair_span(&pair);
    let line = span.start_line;
    let mut is_const = false;
    let mut is_async = false;
    let mut target = None;
    let mut return_type = None;
    let mut names = Vec::new();
    let mut params = Vec::new();
    let mut body = None;
    let mut is_static = false;
    let mut is_override = false;
    let mut attrs = Vec::new();
    let mut type_params = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::template_head => {
                for p in inner.into_inner() {
                    if p.as_rule() == Rule::template_param {
                        type_params.push(parse_template_param(p));
                    }
                }
            }
            Rule::attr => {
                let mut name = String::new();
                let mut arg = None;
                for p in inner.into_inner() {
                    match p.as_rule() {
                        Rule::ident => name = p.as_str().to_string(),
                        Rule::string => arg = Some(unquote(p.as_str())),
                        _ => {}
                    }
                }
                if name == "server" || name == "client" || name == "plugin" {
                    target = Some(name.clone());
                }
                attrs.push(crate::ast::Attr { name, arg });
            }
            Rule::specifiers => {
                let text = inner.as_str();
                is_const = text.contains("const") || text.contains("constexpr");
                is_async = text.contains("async");
                is_static = text.contains("static");
                is_override = text.contains("override");
            }
            Rule::type_spec => return_type = Some(parse_type(inner)),
            Rule::ident => names.push(inner.as_str().to_string()),
            Rule::param_list => params = parse_params(inner),
            Rule::block => body = Some(parse_block(inner)),
            _ => {}
        }
    }
    let (owner, name) = if names.len() >= 2 {
        (Some(names[0].clone()), names[1].clone())
    } else {
        (None, names.first().cloned().unwrap_or_else(|| "anon".into()))
    };
    let func = Function {
        name,
        owner: owner.clone(),
        return_type,
        params,
        body: body.clone().unwrap_or_default(),
        is_const,
        is_async,
        target,
        span,
        doc: None,
        is_static,
        is_override,
        visibility: None,
        type_params,
        attrs,
        parent: owner,
        line,
    };
    if body.is_some() {
        Item::Function(func)
    } else {
        Item::Proto(func)
    }
}

fn parse_var_decl(pair: Pair<Rule>, owner: Option<&str>) -> Decl {
    let span = pair_span(&pair);
    let line = span.start_line;
    let mut is_const = false;
    let mut is_observable = false;
    let mut value_type = None;
    let mut name = "anon".to_string();
    let mut value = None;
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::specifiers => {
                let text = inner.as_str();
                is_const = text.contains("const") || text.contains("constexpr");
                is_observable = text.contains("observable");
            }
            Rule::type_spec => value_type = Some(parse_type(inner)),
            Rule::ident => name = inner.as_str().to_string(),
            Rule::expr => value = Some(parse_expr(inner)),
            _ => {}
        }
    }
    Decl {
        name,
        value_type,
        value,
        is_const,
        is_observable,
        owner: owner.map(str::to_string),
        line,
        span,
        doc: None,
        visibility: None,
    }
}

fn parse_destructure_parts(pair: Pair<Rule>) -> (Vec<String>, Expr) {
    let mut names = Vec::new();
    let mut value = Expr::Null;
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::ident => names.push(inner.as_str().to_string()),
            Rule::expr => value = parse_expr(inner),
            _ => {}
        }
    }
    (names, value)
}

fn parse_type(pair: Pair<Rule>) -> String {
    pair.as_str().split_whitespace().collect::<String>().trim_end_matches('*').to_string()
}

fn parse_template_param(pair: Pair<Rule>) -> TypeParam {
    let idents: Vec<String> = pair
        .into_inner()
        .filter(|p| p.as_rule() == Rule::ident)
        .map(|p| p.as_str().to_string())
        .collect();
    TypeParam {
        name: idents.first().cloned().unwrap_or_default(),
        bound: idents.get(1).cloned(),
    }
}

fn parse_generic_args(pair: Pair<Rule>) -> Vec<String> {
    pair.into_inner()
        .filter(|p| p.as_rule() == Rule::type_spec)
        .map(parse_type)
        .collect()
}

fn parse_params(pair: Pair<Rule>) -> Vec<Param> {
    pair.into_inner()
        .filter(|p| p.as_rule() == Rule::param)
        .map(|param| {
            let mut ty = None;
            let mut name = "arg".to_string();
            let mut default = None;
            for inner in param.into_inner() {
                match inner.as_rule() {
                    Rule::type_spec => ty = Some(parse_type(inner)),
                    Rule::ident => name = inner.as_str().to_string(),
                    Rule::expr => default = Some(parse_expr(inner)),
                    _ => {}
                }
            }
            Param {
                name,
                value_type: ty,
                default,
            }
        })
        .collect()
}

fn parse_block(pair: Pair<Rule>) -> Vec<Stmt> {
    pair.into_inner()
        .filter(|p| p.as_rule() == Rule::stmt)
        .map(parse_stmt)
        .collect()
}

fn parse_stmt(pair: Pair<Rule>) -> Stmt {
    let inner = pair.into_inner().next().expect("stmt inner");
    match inner.as_rule() {
        Rule::if_stmt => parse_if(inner),
        Rule::guard_stmt => parse_guard(inner),
        Rule::match_stmt => parse_match(inner),
        Rule::switch_stmt => parse_switch(inner),
        Rule::for_stmt => parse_for(inner),
        Rule::while_stmt => parse_while(inner),
        Rule::spawn_stmt => Stmt::Spawn {
            body: parse_block(inner.into_inner().find(|p| p.as_rule() == Rule::block).expect("spawn block")),
            parallel: false,
        },
        Rule::parallel_stmt => Stmt::Spawn {
            body: parse_block(inner.into_inner().find(|p| p.as_rule() == Rule::block).expect("parallel block")),
            parallel: true,
        },
        Rule::return_stmt => {
            let values: Vec<Expr> = inner
                .into_inner()
                .filter(|p| p.as_rule() == Rule::expr)
                .map(parse_expr)
                .collect();
            match values.len() {
                0 => Stmt::Return(None),
                1 => Stmt::Return(Some(values.into_iter().next().unwrap())),
                _ => Stmt::Return(Some(Expr::Tuple(values))),
            }
        }
        Rule::empty_stmt => Stmt::Block(Vec::new()),
        Rule::break_stmt => Stmt::Break,
        Rule::continue_stmt => Stmt::Continue,
        Rule::do_while_stmt => parse_do_while(inner),
        Rule::try_stmt => parse_try(inner),
        Rule::delay_stmt => parse_delay(inner),
        Rule::defer_stmt => parse_defer(inner),
        Rule::comptime_stmt => parse_comptime(inner),
        Rule::field_destructure => {
            let (names, value) = parse_field_destructure_parts(inner);
            Stmt::FieldDestructure { names, value }
        }
        Rule::destructure => {
            let (names, value) = parse_destructure_parts(inner);
            Stmt::Destructure { names, value }
        }
        Rule::var_decl => Stmt::Decl(parse_var_decl(inner, None)),
        Rule::expr_stmt => {
            let expr = inner.into_inner().next().map(parse_expr).unwrap_or(Expr::Null);
            Stmt::Expr(expr)
        }
        Rule::block => Stmt::Block(parse_block(inner)),
        _ => Stmt::Block(Vec::new()),
    }
}

fn parse_if(pair: Pair<Rule>) -> Stmt {
    let mut test = Expr::Null;
    let mut stmts = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::expr => test = parse_expr(inner),
            Rule::stmt => stmts.push(parse_stmt(inner)),
            _ => {}
        }
    }
    let consequent = stmts
        .first()
        .cloned()
        .map(stmt_as_list)
        .unwrap_or_default();
    let alternate = stmts.get(1).cloned().map(stmt_as_list);
    Stmt::If {
        test,
        consequent,
        alternate,
    }
}

fn parse_guard(pair: Pair<Rule>) -> Stmt {
    let mut test = Expr::Null;
    let mut body = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::expr => test = parse_expr(inner),
            Rule::stmt => body = stmt_as_list(parse_stmt(inner)),
            _ => {}
        }
    }
    Stmt::Guard { test, body }
}

fn parse_match(pair: Pair<Rule>) -> Stmt {
    let mut discriminant = Expr::Null;
    let mut arms = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::expr => discriminant = parse_expr(inner),
            Rule::match_arm => arms.push(parse_match_arm(inner)),
            _ => {}
        }
    }
    Stmt::Match {
        discriminant,
        arms,
    }
}

fn parse_match_arm(pair: Pair<Rule>) -> crate::ast::MatchArm {
    let mut class_name = None;
    let mut binding = None;
    let mut body = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::match_pat => {
                let text = inner.as_str().trim();
                if text == "_" {
                    class_name = None;
                    binding = None;
                    continue;
                }
                let parts: Vec<_> = inner.into_inner().collect();
                if parts.len() == 1 && parts[0].as_rule() == Rule::ident {
                    class_name = Some(parts[0].as_str().to_string());
                    continue;
                }
                for part in parts {
                    match part.as_rule() {
                        Rule::type_spec => class_name = Some(parse_type(part)),
                        Rule::ident => binding = Some(part.as_str().to_string()),
                        _ => {}
                    }
                }
            }
            Rule::match_body => {
                if let Some(content) = inner.into_inner().next() {
                    if content.as_rule() == Rule::block {
                        body = parse_block(content);
                    } else {
                        body = vec![Stmt::Expr(parse_expr(content))];
                    }
                }
            }
            _ => {}
        }
    }
    crate::ast::MatchArm {
        class_name,
        binding,
        body,
    }
}

fn parse_do_while(pair: Pair<Rule>) -> Stmt {
    let mut test = Expr::Null;
    let mut body = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::stmt => body = stmt_as_list(parse_stmt(inner)),
            Rule::expr => test = parse_expr(inner),
            _ => {}
        }
    }
    Stmt::DoWhile { body, test }
}

fn parse_try(pair: Pair<Rule>) -> Stmt {
    let mut blocks = Vec::new();
    let mut err_name = "err".into();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::block => blocks.push(parse_block(inner)),
            Rule::ident => err_name = inner.as_str().to_string(),
            _ => {}
        }
    }
    let body = blocks.first().cloned().unwrap_or_default();
    let catch = blocks.get(1).cloned().unwrap_or_default();
    Stmt::Try {
        body,
        err_name,
        catch,
    }
}

fn parse_delay(pair: Pair<Rule>) -> Stmt {
    let mut time = Expr::Number("0".into());
    let mut body = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::expr => time = parse_expr(inner),
            Rule::block => body = parse_block(inner),
            _ => {}
        }
    }
    Stmt::Delay { time, body }
}

fn parse_defer(pair: Pair<Rule>) -> Stmt {
    let body = pair
        .into_inner()
        .find(|p| p.as_rule() == Rule::block)
        .map(parse_block)
        .unwrap_or_default();
    Stmt::Defer { body }
}

fn parse_comptime(pair: Pair<Rule>) -> Stmt {
    let span = pair_span(&pair);
    let body = pair
        .into_inner()
        .find(|p| p.as_rule() == Rule::block)
        .map(parse_block)
        .unwrap_or_default();
    Stmt::Comptime { body, span }
}

fn parse_while(pair: Pair<Rule>) -> Stmt {
    let mut test = Expr::Null;
    let mut body = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::expr => test = parse_expr(inner),
            Rule::stmt => body = stmt_as_list(parse_stmt(inner)),
            _ => {}
        }
    }
    Stmt::While { test, body }
}

fn parse_for(pair: Pair<Rule>) -> Stmt {
    let inner = pair.into_inner().next().expect("for kind");
    match inner.as_rule() {
        Rule::c_for_stmt => parse_c_for(inner),
        _ => parse_range_for(inner),
    }
}

fn parse_range_for(pair: Pair<Rule>) -> Stmt {
    let span = pair_span(&pair);
    let mut name = "it".to_string();
    let mut elem_type = None;
    let mut body = Vec::new();
    let mut exprs = Vec::new();
    let mut dots = "..".to_string();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::type_spec => elem_type = Some(parse_type(inner)),
            Rule::ident => name = inner.as_str().to_string(),
            Rule::range_src => {
                for p in inner.into_inner() {
                    match p.as_rule() {
                        Rule::expr => exprs.push(parse_expr(p)),
                        Rule::range_dots => dots = p.as_str().to_string(),
                        _ => {}
                    }
                }
            }
            Rule::expr => exprs.push(parse_expr(inner)),
            Rule::stmt => body = stmt_as_list(parse_stmt(inner)),
            _ => {}
        }
    }
    let iter = match exprs.len() {
        0 => Expr::Null,
        1 => exprs.remove(0),
        2 => Expr::Binary {
            op: dots,
            left: Box::new(exprs.remove(0)),
            right: Box::new(exprs.remove(0)),
        },
        _ => {
            let start = exprs.remove(0);
            let end = exprs.remove(0);
            let step = exprs.remove(0);
            Expr::Binary {
                op: "by".into(),
                left: Box::new(Expr::Binary {
                    op: dots,
                    left: Box::new(start),
                    right: Box::new(end),
                }),
                right: Box::new(step),
            }
        }
    };
    Stmt::ForEach {
        name,
        elem_type,
        iter,
        body,
        span,
    }
}

fn parse_c_for(pair: Pair<Rule>) -> Stmt {
    let mut init = None;
    let mut exprs = Vec::new();
    let mut body = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::c_for_init => {
                init = Some(Box::new(Stmt::Decl(parse_var_like(inner))));
            }
            Rule::expr => exprs.push(parse_expr(inner)),
            Rule::stmt => body = stmt_as_list(parse_stmt(inner)),
            _ => {}
        }
    }
    let test = exprs.first().cloned();
    let incr = exprs.get(1).cloned();
    Stmt::CFor {
        init,
        test,
        incr,
        body,
    }
}

fn parse_var_like(pair: Pair<Rule>) -> Decl {
    parse_var_decl(pair, None)
}

fn parse_switch(pair: Pair<Rule>) -> Stmt {
    let mut inner = pair.into_inner();
    let discriminant = parse_expr(inner.next().expect("switch disc"));
    let mut cases = Vec::new();
    let mut pending: Option<SwitchCase> = None;
    for clause in inner {
        if clause.as_rule() != Rule::switch_clause {
            continue;
        }
        let mut parts = clause.into_inner();
        let Some(head) = parts.next() else {
            continue;
        };
        match head.as_rule() {
            Rule::case_clause => {
                let value = head
                    .into_inner()
                    .find(|p| p.as_rule() == Rule::expr)
                    .map(parse_expr)
                    .unwrap_or(Expr::Null);
                let body: Vec<Stmt> = parts
                    .filter(|p| p.as_rule() == Rule::stmt)
                    .map(parse_stmt)
                    .collect();
                if body.is_empty() {
                    if let Some(current) = pending.as_mut() {
                        if !current.is_default {
                            current.values.push(value);
                            continue;
                        }
                    }
                    if let Some(prev) = pending.take() {
                        cases.push(prev);
                    }
                    pending = Some(SwitchCase {
                        values: vec![value],
                        body: Vec::new(),
                        is_default: false,
                    });
                } else {
                    if let Some(prev) = pending.take() {
                        cases.push(prev);
                    }
                    cases.push(SwitchCase {
                        values: vec![value],
                        body,
                        is_default: false,
                    });
                }
            }
            Rule::default_clause => {
                if let Some(prev) = pending.take() {
                    cases.push(prev);
                }
                let body = parts
                    .filter(|p| p.as_rule() == Rule::stmt)
                    .map(parse_stmt)
                    .collect();
                cases.push(SwitchCase {
                    values: Vec::new(),
                    body,
                    is_default: true,
                });
            }
            _ => {}
        }
    }
    if let Some(prev) = pending.take() {
        cases.push(prev);
    }
    Stmt::Switch {
        discriminant,
        cases,
    }
}

fn stmt_as_list(stmt: Stmt) -> Vec<Stmt> {
    match stmt {
        Stmt::Block(body) => body,
        other => vec![other],
    }
}

fn parse_expr(pair: Pair<Rule>) -> Expr {
    match pair.as_rule() {
        Rule::expr | Rule::assign => parse_assign(pair),
        Rule::try_expr => parse_try_expr(pair),
        Rule::ternary => parse_ternary(pair),
        Rule::coalesce => parse_coalesce(pair),
        Rule::shift => fold_silent(pair, "<<"),
        Rule::or_expr => fold_silent(pair, "||"),
        Rule::and_expr => fold_silent(pair, "&&"),
        Rule::cmp => fold_named_op(pair),
        Rule::concat => fold_silent(pair, ".:"),
        Rule::add | Rule::mul => fold_named_op(pair),
        Rule::pow => parse_pow(pair),
        Rule::unary => parse_unary(pair),
        Rule::colon_expr => parse_colon_expr(pair),
        Rule::primary => parse_primary(pair),
        Rule::atom => parse_atom(pair),
        _ => parse_atom_or_inner(pair),
    }
}

/// `ternary` then optional Result-try (`?`), so ternary always wins for `? … : …`.
fn parse_try_expr(pair: Pair<Rule>) -> Expr {
    let mut inner = pair.into_inner();
    let Some(first) = inner.next() else {
        return Expr::Null;
    };
    let mut node = parse_expr(first);
    if inner.any(|p| p.as_rule() == Rule::try_op) {
        node = Expr::Try {
            argument: Box::new(node),
        };
    }
    node
}

fn parse_ternary(pair: Pair<Rule>) -> Expr {
    let mut inner = pair.into_inner();
    let Some(first) = inner.next() else {
        return Expr::Null;
    };
    let cond = parse_expr(first);
    let Some(then_p) = inner.next() else {
        return cond;
    };
    let then_expr = parse_expr(then_p);
    let else_expr = inner.next().map(parse_expr).unwrap_or(Expr::Null);
    Expr::Ternary {
        cond: Box::new(cond),
        then_expr: Box::new(then_expr),
        else_expr: Box::new(else_expr),
    }
}

fn parse_coalesce(pair: Pair<Rule>) -> Expr {
    let mut inner = pair.into_inner();
    let Some(first) = inner.next() else {
        return Expr::Null;
    };
    let mut node = parse_expr(first);
    for next in inner {
        node = Expr::Coalesce {
            left: Box::new(node),
            right: Box::new(parse_expr(next)),
        };
    }
    node
}

fn parse_pow(pair: Pair<Rule>) -> Expr {
    let mut inner = pair.into_inner();
    let Some(first) = inner.next() else {
        return Expr::Null;
    };
    let left = parse_expr(first);
    let Some(op_or_right) = inner.next() else {
        return left;
    };
    let right = if op_or_right.as_rule() == Rule::pow_op {
        inner.next().map(parse_expr).unwrap_or(Expr::Null)
    } else {
        parse_expr(op_or_right)
    };
    Expr::Binary {
        op: "**".into(),
        left: Box::new(left),
        right: Box::new(right),
    }
}

fn parse_assign(pair: Pair<Rule>) -> Expr {
    let line = pair.line_col().0;
    let mut inner = pair.into_inner();
    let Some(first) = inner.next() else {
        return Expr::Null;
    };
    let left = parse_expr(first);
    let Some(op_or_right) = inner.next() else {
        return left;
    };
    if op_or_right.as_rule() == Rule::assign_op {
        let op = op_or_right.as_str().to_string();
        let right = inner.next().map(parse_expr).unwrap_or(Expr::Null);
        Expr::Assign {
            op,
            left: Box::new(left),
            right: Box::new(right),
            line,
        }
    } else {
        Expr::Assign {
            op: "=".into(),
            left: Box::new(left),
            right: Box::new(parse_expr(op_or_right)),
            line,
        }
    }
}

fn fold_silent(pair: Pair<Rule>, op: &str) -> Expr {
    let mut inner = pair.into_inner();
    let Some(first) = inner.next() else {
        return Expr::Null;
    };
    let mut left = parse_expr(first);
    for right in inner {
        left = Expr::Binary {
            op: op.to_string(),
            left: Box::new(left),
            right: Box::new(parse_expr(right)),
        };
    }
    left
}

fn fold_named_op(pair: Pair<Rule>) -> Expr {
    let mut inner = pair.into_inner();
    let Some(first) = inner.next() else {
        return Expr::Null;
    };
    let mut left = parse_expr(first);
    loop {
        let Some(op_pair) = inner.next() else { break };
        let Some(right) = inner.next() else { break };
        left = Expr::Binary {
            op: op_pair.as_str().to_string(),
            left: Box::new(left),
            right: Box::new(parse_expr(right)),
        };
    }
    left
}

fn parse_unary(pair: Pair<Rule>) -> Expr {
    let mut inner = pair.into_inner();
    let first = inner.next().expect("unary");
    if first.as_rule() == Rule::KW_AWAIT {
        let argument = parse_expr(inner.next().expect("await arg"));
        Expr::Await {
            argument: Box::new(argument),
        }
    } else if first.as_rule() == Rule::unary_op {
        let op = first.as_str().to_string();
        let argument = parse_expr(inner.next().expect("unary arg"));
        Expr::Unary {
            op,
            argument: Box::new(argument),
        }
    } else {
        parse_expr(first)
    }
}

fn parse_colon_expr(pair: Pair<Rule>) -> Expr {
    let mut inner = pair.into_inner();
    let Some(first) = inner.next() else {
        return Expr::Null;
    };
    let mut node = parse_expr(first);
    for suffix in inner {
        node = apply_postfix(node, suffix);
    }
    node
}

fn parse_primary(pair: Pair<Rule>) -> Expr {
    let mut inner = pair.into_inner();
    let Some(atom) = inner.next() else {
        return Expr::Null;
    };
    let mut node = parse_atom(atom);
    for suffix in inner {
        node = apply_postfix(node, suffix);
    }
    node
}

fn parse_atom(pair: Pair<Rule>) -> Expr {
    match pair.as_rule() {
        Rule::atom => {
            if let Some(inner) = pair.into_inner().next() {
                parse_atom(inner)
            } else {
                Expr::Null
            }
        }
        Rule::lambda => parse_lambda(pair),
        Rule::new_expr => {
            let mut class_name = String::new();
            let mut args = Vec::new();
            for inner in pair.into_inner() {
                match inner.as_rule() {
                    Rule::ident => class_name = inner.as_str().to_string(),
                    Rule::call_args => args = parse_call_args(inner),
                    _ => {}
                }
            }
            Expr::New { class_name, args }
        }
        Rule::generic_call => {
            let mut name = String::new();
            let mut type_args = Vec::new();
            let mut args = Vec::new();
            for inner in pair.into_inner() {
                match inner.as_rule() {
                    Rule::ident => name = inner.as_str().to_string(),
                    Rule::type_generic => type_args = parse_generic_args(inner),
                    Rule::call_args => args = parse_call_args(inner),
                    _ => {}
                }
            }
            Expr::Call {
                object: None,
                name,
                args,
                access: ".".into(),
                type_args,
            }
        }
        Rule::named_cast => parse_named_cast(pair),
        Rule::boolean => Expr::Bool(pair.as_str().contains("true")),
        Rule::null_lit => Expr::Null,
        Rule::init_list => parse_init_list(pair),
        Rule::at_sigil => parse_at_sigil(pair),
        Rule::ident => match pair.as_str() {
            "true" => Expr::Bool(true),
            "false" => Expr::Bool(false),
            "null" | "nullptr" => Expr::Null,
            name => Expr::Ident(name.to_string()),
        },
        Rule::number => Expr::Number(pair.as_str().to_string()),
        Rule::raw_string => Expr::String(unraw(pair.as_str())),
        Rule::string | Rule::char_string => Expr::String(unquote(pair.as_str())),
        Rule::template_string => parse_template(pair),
        Rule::string_join => parse_string_join(pair),
        Rule::expr => parse_expr(pair),
        _ => match pair.as_str() {
            "true" => Expr::Bool(true),
            "false" => Expr::Bool(false),
            "null" | "nullptr" => Expr::Null,
            other if other.starts_with('"') || other.starts_with('\'') || other.starts_with('`') => {
                Expr::String(unquote(other))
            }
            _ => parse_atom_or_inner(pair),
        },
    }
}

fn parse_at_sigil(pair: Pair<Rule>) -> Expr {
    let line = pair.line_col().0;
    let name = pair
        .into_inner()
        .find(|p| p.as_rule() == Rule::ident)
        .map(|p| p.as_str().to_string())
        .unwrap_or_default();
    if name == "this" {
        Expr::This { line }
    } else {
        Expr::AtField { name, line }
    }
}

fn parse_named_cast(pair: Pair<Rule>) -> Expr {
    let mut value_type = "any".to_string();
    let mut argument = Expr::Null;
    let mut kind = "static_cast".to_string();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::named_cast_kw => kind = inner.as_str().to_string(),
            Rule::type_spec => value_type = parse_type(inner),
            Rule::expr => argument = parse_expr(inner),
            _ => {}
        }
    }
    Expr::Cast {
        value_type,
        argument: Box::new(argument),
        kind,
    }
}

fn parse_atom_or_inner(pair: Pair<Rule>) -> Expr {
    if let Some(inner) = pair.clone().into_inner().next() {
        parse_expr(inner)
    } else {
        Expr::Ident(pair.as_str().to_string())
    }
}

fn apply_postfix(node: Expr, suffix: Pair<Rule>) -> Expr {
    if suffix.as_rule() == Rule::postfix {
        let inner = suffix.into_inner().next().expect("postfix inner");
        return apply_postfix(node, inner);
    }
    match suffix.as_rule() {
        Rule::colon_suf => apply_named_suffix(node, suffix, ":"),
        Rule::dot_suf => apply_named_suffix(node, suffix, "."),
        Rule::scope_suf => apply_named_suffix(node, suffix, "::"),
        Rule::cleanup_suf => apply_named_suffix(node, suffix, "~>"),
        Rule::index_suf => {
            let index = suffix
                .into_inner()
                .find(|p| p.as_rule() == Rule::expr)
                .map(parse_expr)
                .unwrap_or(Expr::Null);
            Expr::Index {
                object: Box::new(node),
                index: Box::new(index),
            }
        }
        Rule::qmark_dot_suf => {
            let mut name = String::new();
            let mut args = None;
            for inner in suffix.into_inner() {
                match inner.as_rule() {
                    Rule::ident => name = inner.as_str().to_string(),
                    Rule::call_args => args = Some(parse_call_args(inner)),
                    _ => {}
                }
            }
            Expr::OptionalChain {
                object: Box::new(node),
                name,
                args,
            }
        }
        Rule::as_suf => {
            let value_type = suffix
                .into_inner()
                .find(|p| p.as_rule() == Rule::type_spec)
                .map(parse_type)
                .unwrap_or_else(|| "any".into());
            Expr::Cast {
                value_type,
                argument: Box::new(node),
                kind: "as".into(),
            }
        }
        Rule::inc_suf => Expr::Update {
            op: suffix.as_str().to_string(),
            target: Box::new(node),
        },
        Rule::init_list | Rule::init_suf => parse_init_list(suffix),
        Rule::call_args => {
            let args = parse_call_args(suffix);
            match node {
                Expr::Ident(name) => Expr::Call {
                    object: None,
                    name,
                    args,
                    access: ".".to_string(),
                    type_args: Vec::new(),
                },
                Expr::AtField { name, .. } => Expr::Call {
                    object: Some(Box::new(Expr::This { line: 0 })),
                    name,
                    args,
                    access: ":".to_string(),
                    type_args: Vec::new(),
                },
                other => Expr::Call {
                    object: Some(Box::new(other)),
                    name: "call".into(),
                    args,
                    access: ".".to_string(),
                    type_args: Vec::new(),
                },
            }
        }
        _ => node,
    }
}

fn apply_named_suffix(node: Expr, suffix: Pair<Rule>, access: &str) -> Expr {
    let mut name = String::new();
    let mut type_args = Vec::new();
    let mut args = None;
    for inner in suffix.into_inner() {
        match inner.as_rule() {
            Rule::ident => name = inner.as_str().to_string(),
            Rule::type_generic => type_args = parse_generic_args(inner),
            Rule::call_args => args = Some(parse_call_args(inner)),
            _ => {}
        }
    }
    if let Some(args) = args {
        Expr::Call {
            object: Some(Box::new(node)),
            name,
            args,
            access: access.to_string(),
            type_args,
        }
    } else {
        Expr::Member {
            object: Box::new(node),
            name,
            access: access.to_string(),
        }
    }
}

fn parse_call_args(pair: Pair<Rule>) -> Vec<Expr> {
    pair.into_inner()
        .find(|p| p.as_rule() == Rule::args)
        .map(|args| {
            args.into_inner()
                .filter(|p| p.as_rule() == Rule::expr)
                .map(parse_expr)
                .collect()
        })
        .unwrap_or_default()
}

fn parse_init_list(pair: Pair<Rule>) -> Expr {
    let mut current = pair;
    loop {
        match current.as_rule() {
            Rule::init_suf | Rule::init_list | Rule::init_body => {
                match current.into_inner().next() {
                    Some(inner) => current = inner,
                    None => return Expr::ArrayLit { elements: Vec::new() },
                }
            }
            Rule::designated_fields => {
                return Expr::InitList {
                    fields: parse_designated(current),
                };
            }
            Rule::dict_pairs => {
                return Expr::DictLit {
                    pairs: parse_dict_pairs(current),
                };
            }
            Rule::expr_list => {
                return Expr::ArrayLit {
                    elements: current
                        .into_inner()
                        .filter(|p| p.as_rule() == Rule::expr)
                        .map(parse_expr)
                        .collect(),
                };
            }
            Rule::expr => return parse_expr(current),
            _ => return parse_atom_or_inner(current),
        }
    }
}

fn parse_dict_pairs(pair: Pair<Rule>) -> Vec<(Expr, Expr)> {
    pair.into_inner()
        .filter(|p| p.as_rule() == Rule::dict_pair)
        .map(|field| {
            let mut inner = field
                .into_inner()
                .filter(|p| p.as_rule() == Rule::expr);
            let key = inner.next().map(parse_expr).unwrap_or(Expr::Null);
            let value = inner.next().map(parse_expr).unwrap_or(Expr::Null);
            (key, value)
        })
        .collect()
}

fn parse_designated(pair: Pair<Rule>) -> Vec<(String, Expr)> {
    pair.into_inner()
        .filter(|p| p.as_rule() == Rule::designated_field)
        .map(|field| {
            let mut inner = field.into_inner();
            let name = inner.next().map(|p| p.as_str().to_string()).unwrap_or_default();
            let value = inner.next().map(parse_expr).unwrap_or(Expr::Null);
            (name, value)
        })
        .collect()
}

fn parse_lambda(pair: Pair<Rule>) -> Expr {
    let mut params = Vec::new();
    let mut body = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::param_list => params = parse_params(inner),
            Rule::block => body = parse_block(inner),
            _ => {}
        }
    }
    Expr::Lambda { params, body }
}

fn parse_template(pair: Pair<Rule>) -> Expr {
    let mut parts = Vec::new();
    collect_template(pair, &mut parts);
    if parts.len() == 1 {
        if let InterpPart::Text(text) = &parts[0] {
            return Expr::String(text.clone());
        }
    }
    if parts.is_empty() {
        return Expr::String(String::new());
    }
    Expr::Interp { parts }
}

fn collect_template(pair: Pair<Rule>, parts: &mut Vec<InterpPart>) {
    match pair.as_rule() {
        Rule::template_string | Rule::template_chunk => {
            for inner in pair.into_inner() {
                collect_template(inner, parts);
            }
        }
        Rule::template_text => parts.push(InterpPart::Text(unescape(pair.as_str()))),
        Rule::template_escaped => parts.push(InterpPart::Text(unescape(pair.as_str()))),
        Rule::template_interp => {
            if let Some(expr) = pair.into_inner().find(|p| p.as_rule() == Rule::expr) {
                parts.push(InterpPart::Value(parse_expr(expr)));
            }
        }
        _ => {
            for inner in pair.into_inner() {
                collect_template(inner, parts);
            }
        }
    }
}

fn parse_string_join(pair: Pair<Rule>) -> Expr {
    let mut parts = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::template_string => collect_template(inner, &mut parts),
            Rule::expr => match parse_expr(inner) {
                Expr::Interp { parts: more } => parts.extend(more),
                Expr::String(text) => parts.push(InterpPart::Text(text)),
                other => parts.push(InterpPart::Value(other)),
            },
            _ => {}
        }
    }
    Expr::Interp { parts }
}

fn unraw(raw: &str) -> String {
    let s = raw.trim();
    if let Some(rest) = s.strip_prefix("R\"(").and_then(|r| r.strip_suffix(")\"")) {
        rest.to_string()
    } else {
        unquote(s)
    }
}

fn unquote(raw: &str) -> String {
    let bytes = raw.as_bytes();
    if bytes.len() >= 2 {
        let first = bytes[0];
        let last = bytes[bytes.len() - 1];
        if (first == b'"' && last == b'"')
            || (first == b'\'' && last == b'\'')
            || (first == b'`' && last == b'`')
        {
            return unescape(&raw[1..raw.len() - 1]);
        }
    }
    unescape(raw)
}

fn unescape(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    let mut chars = text.chars();
    while let Some(ch) = chars.next() {
        if ch == '\\' {
            match chars.next() {
                Some('n') => out.push('\n'),
                Some('t') => out.push('\t'),
                Some('r') => out.push('\r'),
                Some('`') => out.push('`'),
                Some('{') => out.push('{'),
                Some('}') => out.push('}'),
                Some(other) => out.push(other),
                None => out.push('\\'),
            }
        } else {
            out.push(ch);
        }
    }
    out
}
