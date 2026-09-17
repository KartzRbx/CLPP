use crate::ast::*;
use crate::error::ClppError;
use miette::Result;
use pest::error::ErrorVariant;
use pest::iterators::Pair;
use pest::Parser as PestParser;
use pest_derive::Parser;

#[derive(Parser)]
#[grammar = "src/parser/grammar.pest"]
pub struct ClppParser;

pub fn parse(source: &str, file_name: &str) -> Result<Program> {
    let mut pairs = ClppParser::parse(Rule::file, source).map_err(|err| {
        let (line, col) = match err.line_col {
            pest::error::LineColLocation::Pos((l, c)) => (l, c),
            pest::error::LineColLocation::Span((l, c), _) => (l, c),
        };
        ClppError::at_line(source, line, col, format_parse_error(&err))
    })?;
    let file = pairs.next().expect("file pair");
    let mut items = Vec::new();
    for pair in file.into_inner() {
        if pair.as_rule() == Rule::item {
            items.extend(parse_item(pair));
        }
    }
    Ok(Program {
        items,
        file_name: file_name.to_string(),
    })
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
    }
    raw.lines()
        .last()
        .map(|l| l.trim().trim_start_matches('=').trim())
        .filter(|l| !l.is_empty())
        .map(|l| l.to_string())
        .unwrap_or(raw)
}

fn parse_item(pair: Pair<Rule>) -> Vec<Item> {
    let inner = pair.into_inner().next().expect("item inner");
    match inner.as_rule() {
        Rule::struct_decl => parse_struct(inner, None),
        Rule::function_item => vec![parse_function(inner)],
        Rule::destructure => {
            let (names, value) = parse_destructure_parts(inner);
            vec![Item::Destructure { names, value }]
        }
        Rule::var_decl => vec![Item::Decl(parse_var_decl(inner, None))],
        Rule::namespace_item => inner
            .into_inner()
            .filter(|p| p.as_rule() == Rule::item)
            .flat_map(parse_item)
            .collect(),
        _ => Vec::new(),
    }
}

fn parse_struct(pair: Pair<Rule>, parent: Option<&str>) -> Vec<Item> {
    let mut items = Vec::new();
    let mut name = "_anon".to_string();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::ident => name = inner.as_str().to_string(),
            Rule::struct_member => items.extend(parse_struct_member(inner, &name)),
            _ => {}
        }
    }
    let _ = parent;
    items
}

fn parse_struct_member(pair: Pair<Rule>, owner: &str) -> Vec<Item> {
    let inner = pair.into_inner().next().expect("member");
    match inner.as_rule() {
        Rule::nested_struct => {
            let mut nested = inner.into_inner();
            let Some(decl) = nested.next() else {
                return Vec::new();
            };
            let mut items = parse_struct(decl, Some(owner));
            if let Some(instance) = nested.find(|p| p.as_rule() == Rule::ident) {
                items.push(Item::Decl(Decl {
                    name: instance.as_str().to_string(),
                    value_type: Some(owner.to_string()),
                    value: None,
                    is_const: false,
                    is_observable: false,
                    owner: Some(owner.to_string()),
                    line: instance.line_col().0,
                }));
            }
            items
        }
        Rule::method_proto => {
            let mut func = match parse_function(inner) {
                Item::Function(f) | Item::Proto(f) => f,
                _ => return Vec::new(),
            };
            func.owner = Some(owner.to_string());
            vec![Item::Proto(func)]
        }
        Rule::field_decl => vec![Item::Decl(parse_var_decl(inner, Some(owner)))],
        _ => Vec::new(),
    }
}

fn parse_function(pair: Pair<Rule>) -> Item {
    let mut is_const = false;
    let mut is_async = false;
    let mut target = None;
    let mut return_type = None;
    let mut names = Vec::new();
    let mut params = Vec::new();
    let mut body = None;
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::attr => {
                if let Some(ident) = inner.into_inner().find(|p| p.as_rule() == Rule::ident) {
                    target = Some(ident.as_str().to_string());
                }
            }
            Rule::specifiers => {
                let text = inner.as_str();
                is_const = text.contains("const") || text.contains("constexpr");
                is_async = text.contains("async");
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
        owner,
        return_type,
        params,
        body: body.clone().unwrap_or_default(),
        is_const,
        is_async,
        target,
    };
    if body.is_some() {
        Item::Function(func)
    } else {
        Item::Proto(func)
    }
}

fn parse_var_decl(pair: Pair<Rule>, owner: Option<&str>) -> Decl {
    let line = pair.line_col().0;
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
    let mut name = String::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::specifiers => {}
            Rule::type_name => {
                name = inner
                    .as_str()
                    .split_whitespace()
                    .collect::<Vec<_>>()
                    .join("");
            }
            Rule::type_generic => {
                name.push_str(&inner.as_str().split_whitespace().collect::<String>());
            }
            _ => {}
        }
    }
    name.trim_end_matches('*').to_string()
}

fn parse_params(pair: Pair<Rule>) -> Vec<Param> {
    pair.into_inner()
        .filter(|p| p.as_rule() == Rule::param)
        .map(|param| {
            let mut ty = None;
            let mut name = "arg".to_string();
            for inner in param.into_inner() {
                match inner.as_rule() {
                    Rule::type_spec => ty = Some(parse_type(inner)),
                    Rule::ident => name = inner.as_str().to_string(),
                    _ => {}
                }
            }
            Param {
                name,
                value_type: ty,
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
            let value = inner.into_inner().find(|p| p.as_rule() == Rule::expr).map(parse_expr);
            Stmt::Return(value)
        }
        Rule::break_stmt => Stmt::Break,
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
                for part in inner.into_inner() {
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
    let mut name = "it".to_string();
    let mut iter = Expr::Null;
    let mut body = Vec::new();
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::ident => name = inner.as_str().to_string(),
            Rule::expr => iter = parse_expr(inner),
            Rule::stmt => body = stmt_as_list(parse_stmt(inner)),
            _ => {}
        }
    }
    Stmt::ForEach { name, iter, body }
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
        Rule::shift => fold_silent(pair, "<<"),
        Rule::or_expr => fold_silent(pair, "||"),
        Rule::and_expr => fold_silent(pair, "&&"),
        Rule::cmp => fold_named_op(pair),
        Rule::concat => fold_silent(pair, ".:"),
        Rule::add | Rule::mul => fold_named_op(pair),
        Rule::unary => parse_unary(pair),
        Rule::primary => parse_primary(pair),
        Rule::atom => parse_atom(pair),
        _ => parse_atom_or_inner(pair),
    }
}

fn parse_assign(pair: Pair<Rule>) -> Expr {
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
        }
    } else {
        Expr::Assign {
            op: "=".into(),
            left: Box::new(left),
            right: Box::new(parse_expr(op_or_right)),
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
        Rule::get_service => {
            let service = pair
                .into_inner()
                .find(|p| p.as_rule() == Rule::ident)
                .map(|p| p.as_str().to_string())
                .unwrap_or_default();
            Expr::GetService { service }
        }
        Rule::named_cast => parse_named_cast(pair),
        Rule::boolean => Expr::Bool(pair.as_str().contains("true")),
        Rule::null_lit => Expr::Null,
        Rule::init_list => parse_init_list(pair),
        Rule::ident => match pair.as_str() {
            "true" => Expr::Bool(true),
            "false" => Expr::Bool(false),
            "null" | "nullptr" => Expr::Null,
            name => Expr::Ident(name.to_string()),
        },
        Rule::number => Expr::Number(pair.as_str().to_string()),
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

fn parse_named_cast(pair: Pair<Rule>) -> Expr {
    let mut value_type = "any".to_string();
    let mut argument = Expr::Null;
    for inner in pair.into_inner() {
        match inner.as_rule() {
            Rule::type_spec => value_type = parse_type(inner),
            Rule::expr => argument = parse_expr(inner),
            _ => {}
        }
    }
    Expr::Cast {
        value_type,
        argument: Box::new(argument),
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
                },
                other => Expr::Call {
                    object: Some(Box::new(other)),
                    name: "call".into(),
                    args,
                    access: ".".to_string(),
                },
            }
        }
        _ => node,
    }
}

fn apply_named_suffix(node: Expr, suffix: Pair<Rule>, access: &str) -> Expr {
    let mut inner = suffix.into_inner();
    let name = inner
        .next()
        .map(|p| p.as_str().to_string())
        .unwrap_or_default();
    if let Some(args_pair) = inner.find(|p| p.as_rule() == Rule::call_args) {
        Expr::Call {
            object: Some(Box::new(node)),
            name,
            args: parse_call_args(args_pair),
            access: access.to_string(),
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
