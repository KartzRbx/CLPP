//! Constant environment + expression folding.

use crate::ast::Expr;
use std::collections::HashMap;

#[derive(Clone, Debug, Default)]
pub struct ConstEnv {
    /// Proven constant expressions by local / file const name.
    values: HashMap<String, Expr>,
}

impl ConstEnv {
    pub fn bind(&mut self, name: &str, expr: &Expr) {
        let folded = fold_expr(expr, self);
        if is_const_value(&folded) {
            self.values.insert(name.to_string(), folded);
        }
    }

    pub fn get(&self, name: &str) -> Option<&Expr> {
        self.values.get(name)
    }
}

pub fn is_const_value(expr: &Expr) -> bool {
    matches!(
        expr,
        Expr::Number(_) | Expr::Bool(_) | Expr::String(_) | Expr::Null
    )
}

pub fn fold_expr(expr: &Expr, env: &ConstEnv) -> Expr {
    match expr {
        Expr::Ident(name) => env
            .get(name)
            .cloned()
            .unwrap_or_else(|| Expr::Ident(name.clone())),
        Expr::Unary { op, argument } => {
            let argument = fold_expr(argument, env);
            match (op.as_str(), &argument) {
                ("-", Expr::Number(n)) => {
                    if let Some(v) = parse_f64(n) {
                        return number_expr(-v, n.contains('.'));
                    }
                    Expr::Unary {
                        op: op.clone(),
                        argument: Box::new(argument),
                    }
                }
                ("!", Expr::Bool(b)) => Expr::Bool(!b),
                _ => Expr::Unary {
                    op: op.clone(),
                    argument: Box::new(argument),
                },
            }
        }
        Expr::Binary { op, left, right } => {
            let left = fold_expr(left, env);
            let right = fold_expr(right, env);
            if let Some(folded) = fold_binary(op, &left, &right) {
                return folded;
            }
            Expr::Binary {
                op: op.clone(),
                left: Box::new(left),
                right: Box::new(right),
            }
        }
        Expr::Ternary {
            cond,
            then_expr,
            else_expr,
        } => {
            let cond = fold_expr(cond, env);
            match eval_bool(&cond, env) {
                Some(true) => fold_expr(then_expr, env),
                Some(false) => fold_expr(else_expr, env),
                None => Expr::Ternary {
                    cond: Box::new(cond),
                    then_expr: Box::new(fold_expr(then_expr, env)),
                    else_expr: Box::new(fold_expr(else_expr, env)),
                },
            }
        }
        Expr::Call {
            object,
            name,
            args,
            access,
            type_args,
        } => Expr::Call {
            object: object.as_ref().map(|o| Box::new(fold_expr(o, env))),
            name: name.clone(),
            args: args.iter().map(|a| fold_expr(a, env)).collect(),
            access: access.clone(),
            type_args: type_args.clone(),
        },
        Expr::Member {
            object,
            name,
            access,
        } => Expr::Member {
            object: Box::new(fold_expr(object, env)),
            name: name.clone(),
            access: access.clone(),
        },
        Expr::Assign {
            op,
            left,
            right,
            line,
        } => Expr::Assign {
            op: op.clone(),
            left: Box::new(fold_expr(left, env)),
            right: Box::new(fold_expr(right, env)),
            line: *line,
        },
        Expr::Cast {
            value_type,
            argument,
            kind,
        } => Expr::Cast {
            value_type: value_type.clone(),
            argument: Box::new(fold_expr(argument, env)),
            kind: kind.clone(),
        },
        Expr::Await { argument } => Expr::Await {
            argument: Box::new(fold_expr(argument, env)),
        },
        Expr::Update { op, target } => Expr::Update {
            op: op.clone(),
            target: Box::new(fold_expr(target, env)),
        },
        Expr::Index { object, index } => Expr::Index {
            object: Box::new(fold_expr(object, env)),
            index: Box::new(fold_expr(index, env)),
        },
        Expr::Coalesce { left, right } => {
            let left = fold_expr(left, env);
            if matches!(left, Expr::Null) {
                fold_expr(right, env)
            } else if is_const_value(&left) && !matches!(left, Expr::Null) {
                left
            } else {
                Expr::Coalesce {
                    left: Box::new(left),
                    right: Box::new(fold_expr(right, env)),
                }
            }
        }
        Expr::Tuple(values) => Expr::Tuple(values.iter().map(|v| fold_expr(v, env)).collect()),
        Expr::ArrayLit { elements } => Expr::ArrayLit {
            elements: elements.iter().map(|e| fold_expr(e, env)).collect(),
        },
        other => other.clone(),
    }
}

pub fn eval_bool(expr: &Expr, env: &ConstEnv) -> Option<bool> {
    match fold_expr(expr, env) {
        Expr::Bool(b) => Some(b),
        Expr::Number(n) => parse_f64(&n).map(|v| v != 0.0),
        _ => None,
    }
}

#[allow(dead_code)]
pub fn eval_number(expr: &Expr, env: &ConstEnv) -> Option<f64> {
    match fold_expr(expr, env) {
        Expr::Number(n) => parse_f64(&n),
        Expr::Bool(true) => Some(1.0),
        Expr::Bool(false) => Some(0.0),
        _ => None,
    }
}

fn fold_binary(op: &str, left: &Expr, right: &Expr) -> Option<Expr> {
    match op {
        "+" | "-" | "*" | "/" | "%" => {
            let a = match left {
                Expr::Number(n) => parse_f64(n)?,
                _ => return None,
            };
            let b = match right {
                Expr::Number(n) => parse_f64(n)?,
                _ => return None,
            };
            let floatish = matches!(left, Expr::Number(n) if n.contains('.'))
                || matches!(right, Expr::Number(n) if n.contains('.'))
                || op == "/";
            let v = match op {
                "+" => a + b,
                "-" => a - b,
                "*" => a * b,
                "/" => {
                    if b == 0.0 {
                        return None;
                    }
                    a / b
                }
                "%" => {
                    if b == 0.0 {
                        return None;
                    }
                    a % b
                }
                _ => return None,
            };
            Some(number_expr(v, floatish || !v.fract().is_zero_approx()))
        }
        "==" | "!=" | "<" | ">" | "<=" | ">=" => {
            if let (Some(a), Some(b)) = (
                match left {
                    Expr::Number(n) => parse_f64(n),
                    _ => None,
                },
                match right {
                    Expr::Number(n) => parse_f64(n),
                    _ => None,
                },
            ) {
                let r = match op {
                    "==" => a == b,
                    "!=" => a != b,
                    "<" => a < b,
                    ">" => a > b,
                    "<=" => a <= b,
                    ">=" => a >= b,
                    _ => return None,
                };
                return Some(Expr::Bool(r));
            }
            if let (Expr::Bool(a), Expr::Bool(b)) = (left, right) {
                let r = match op {
                    "==" => a == b,
                    "!=" => a != b,
                    _ => return None,
                };
                return Some(Expr::Bool(r));
            }
            None
        }
        "&&" => match (left, right) {
            (Expr::Bool(false), _) => Some(Expr::Bool(false)),
            (Expr::Bool(true), r) => Some(r.clone()),
            _ => None,
        },
        "||" => match (left, right) {
            (Expr::Bool(true), _) => Some(Expr::Bool(true)),
            (Expr::Bool(false), r) => Some(r.clone()),
            _ => None,
        },
        _ => None,
    }
}

fn parse_f64(s: &str) -> Option<f64> {
    s.parse().ok()
}

fn number_expr(v: f64, as_float: bool) -> Expr {
    if !as_float && v.fract().abs() < 1e-12 && v.abs() < 1e15 {
        Expr::Number(format!("{}", v as i64))
    } else if v.fract().abs() < 1e-12 && v.abs() < 1e15 {
        // Prefer integer spelling when exact.
        Expr::Number(format!("{}", v as i64))
    } else {
        let mut s = format!("{v}");
        if !s.contains('.') && !s.contains('e') && !s.contains('E') {
            s.push_str(".0");
        }
        Expr::Number(s)
    }
}

trait FractExt {
    fn is_zero_approx(self) -> bool;
}

impl FractExt for f64 {
    fn is_zero_approx(self) -> bool {
        self.abs() < 1e-12
    }
}
