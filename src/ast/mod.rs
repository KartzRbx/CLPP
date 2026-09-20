mod span;
pub mod visit;

pub use span::{SourceComment, Span};

#[derive(Debug, Clone)]
pub struct Program {
    pub items: Vec<Item>,
    pub file_name: String,
}

#[derive(Debug, Clone)]
pub enum Item {
    Function(Function),
    Proto(Function),
    Decl(Decl),
    Destructure {
        names: Vec<String>,
        value: Expr,
    },
    Unsupported {
        kind: String,
        line: usize,
        message: String,
    },
}

#[derive(Debug, Clone)]
pub struct Function {
    pub name: String,
    pub owner: Option<String>,
    pub return_type: Option<String>,
    pub params: Vec<Param>,
    pub body: Vec<Stmt>,
    pub is_const: bool,
    pub is_async: bool,
    pub target: Option<String>,
    pub line: usize,
    pub span: Span,
    pub doc: Option<String>,
}

#[derive(Debug, Clone)]
pub struct Param {
    pub name: String,
    pub value_type: Option<String>,
}

#[derive(Debug, Clone)]
pub struct Decl {
    pub name: String,
    pub value_type: Option<String>,
    pub value: Option<Expr>,
    pub is_const: bool,
    pub is_observable: bool,
    pub owner: Option<String>,
    pub line: usize,
    pub span: Span,
    pub doc: Option<String>,
}

#[derive(Debug, Clone)]
pub struct SwitchCase {
    pub values: Vec<Expr>,
    pub body: Vec<Stmt>,
    pub is_default: bool,
}

#[derive(Debug, Clone)]
pub struct MatchArm {
    pub class_name: Option<String>,
    pub binding: Option<String>,
    pub body: Vec<Stmt>,
}

#[derive(Debug, Clone)]
pub enum Expr {
    Null,
    Bool(bool),
    Number(String),
    String(String),
    Interp {
        parts: Vec<InterpPart>,
    },
    Ident(String),
    Tuple(Vec<Expr>),
    This {
        line: usize,
    },
    AtField {
        name: String,
        line: usize,
    },
    Unary {
        op: String,
        argument: Box<Expr>,
    },
    Await {
        argument: Box<Expr>,
    },
    Binary {
        op: String,
        left: Box<Expr>,
        right: Box<Expr>,
    },
    Assign {
        op: String,
        left: Box<Expr>,
        right: Box<Expr>,
        line: usize,
    },
    Member {
        object: Box<Expr>,
        name: String,
        access: String,
    },
    Call {
        object: Option<Box<Expr>>,
        name: String,
        args: Vec<Expr>,
        access: String,
    },
    New {
        class_name: String,
        args: Vec<Expr>,
    },
    GetService {
        service: String,
    },
    Cast {
        value_type: String,
        argument: Box<Expr>,
    },
    Lambda {
        params: Vec<Param>,
        body: Vec<Stmt>,
    },
    InitList {
        fields: Vec<(String, Expr)>,
    },
    ArrayLit {
        elements: Vec<Expr>,
    },
    DictLit {
        pairs: Vec<(Expr, Expr)>,
    },
    Update {
        op: String,
        target: Box<Expr>,
    },
}

#[derive(Debug, Clone)]
pub enum InterpPart {
    Text(String),
    Value(Expr),
}

#[derive(Debug, Clone)]
pub enum Stmt {
    Decl(Decl),
    Destructure {
        names: Vec<String>,
        value: Expr,
    },
    Expr(Expr),
    Return(Option<Expr>),
    Break,
    If {
        test: Expr,
        consequent: Vec<Stmt>,
        alternate: Option<Vec<Stmt>>,
    },
    Guard {
        test: Expr,
        body: Vec<Stmt>,
    },
    While {
        test: Expr,
        body: Vec<Stmt>,
    },
    ForEach {
        name: String,
        elem_type: Option<String>,
        iter: Expr,
        body: Vec<Stmt>,
        span: Span,
    },
    CFor {
        init: Option<Box<Stmt>>,
        test: Option<Expr>,
        incr: Option<Expr>,
        body: Vec<Stmt>,
    },
    Switch {
        discriminant: Expr,
        cases: Vec<SwitchCase>,
    },
    Match {
        discriminant: Expr,
        arms: Vec<MatchArm>,
    },
    Spawn {
        body: Vec<Stmt>,
        parallel: bool,
    },
    Block(Vec<Stmt>),
}

#[derive(Debug, Clone)]
pub struct ModuleRequire {
    pub name: String,
    pub from_file: String,
    pub to_file: String,
}

#[derive(Debug, Clone, Default)]
pub struct CompileContext {
    pub strict: bool,
    pub nonstrict: bool,
    pub native: bool,
    pub optimize: Option<u8>,
    pub is_script: bool,
    pub is_header: bool,
    pub script_kind: Option<String>,
    pub libraries: Vec<String>,
    pub requires: Vec<ModuleRequire>,
    /// Expanded line (1-based index into this vec as 0-based) → original source line.
    pub line_map: Vec<usize>,
    pub comments: Vec<SourceComment>,
}

impl Function {
    pub fn span(&self) -> Span {
        self.span
    }
}

impl Decl {
    pub fn span(&self) -> Span {
        self.span
    }
}

impl Stmt {
    pub fn span(&self) -> Span {
        match self {
            Stmt::Decl(decl) => decl.span,
            Stmt::ForEach { span, .. } => *span,
            Stmt::Expr(expr) | Stmt::Return(Some(expr)) | Stmt::Destructure { value: expr, .. } => {
                expr.span()
            }
            Stmt::If { test, .. } | Stmt::Guard { test, .. } | Stmt::While { test, .. } => {
                test.span()
            }
            Stmt::CFor { init, test, .. } => init
                .as_ref()
                .map(|s| s.span())
                .or_else(|| test.as_ref().map(|e| e.span()))
                .unwrap_or_default(),
            Stmt::Switch { discriminant, .. } | Stmt::Match { discriminant, .. } => {
                discriminant.span()
            }
            _ => Span::default(),
        }
    }

    pub fn line(&self) -> usize {
        let span = self.span();
        if span.start_line == 0 {
            1
        } else {
            span.start_line
        }
    }
}

impl Expr {
    pub fn span(&self) -> Span {
        match self {
            Expr::This { line } | Expr::AtField { line, .. } | Expr::Assign { line, .. } => {
                Span::point(*line, 1)
            }
            Expr::Member { object, .. } => object.span(),
            Expr::Call { object, args, .. } => object
                .as_ref()
                .map(|o| o.span())
                .or_else(|| args.first().map(|a| a.span()))
                .unwrap_or_default(),
            Expr::Unary { argument, .. }
            | Expr::Await { argument }
            | Expr::Cast { argument, .. }
            | Expr::Update { target: argument, .. } => argument.span(),
            Expr::Binary { left, .. } => left.span(),
            _ => Span::default(),
        }
    }

    pub fn ident_name(&self) -> Option<&str> {
        match self {
            Expr::Ident(name) => Some(name.as_str()),
            Expr::This { .. } => Some("this"),
            Expr::AtField { name, .. } => Some(name.as_str()),
            _ => None,
        }
    }
}
