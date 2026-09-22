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
    Enum {
        name: String,
        numeric: bool,
        variants: Vec<(String, Option<String>)>,
        line: usize,
        span: Span,
        doc: Option<String>,
    },
    TypeAlias {
        name: String,
        ty: String,
        type_params: Vec<String>,
        line: usize,
        span: Span,
        doc: Option<String>,
    },
    Class {
        name: String,
        parent: Option<String>,
        type_params: Vec<TypeParam>,
        line: usize,
        span: Span,
        doc: Option<String>,
    },
    Import {
        /// Bindings: export name in the module + optional local alias (`as`).
        names: Vec<ImportName>,
        module: String,
        line: usize,
        span: Span,
    },
}

/// Generic type parameter (`T` or `T : Bound`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TypeParam {
    pub name: String,
    /// Nominal interface/struct bound (checked generics, RFC 0010).
    pub bound: Option<String>,
}

impl TypeParam {
    pub fn unbound(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            bound: None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ImportName {
    pub name: String,
    pub alias: Option<String>,
}

impl ImportName {
    pub fn local_name(&self) -> &str {
        self.alias.as_deref().unwrap_or(&self.name)
    }
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
    pub span: Span,
    pub doc: Option<String>,
    pub is_static: bool,
    pub is_override: bool,
    pub visibility: Option<String>,
    pub type_params: Vec<TypeParam>,
    pub attrs: Vec<Attr>,
    pub parent: Option<String>,
    pub line: usize,
}

#[derive(Debug, Clone)]
pub struct Attr {
    pub name: String,
    pub arg: Option<String>,
}

#[derive(Debug, Clone)]
pub struct Param {
    pub name: String,
    pub value_type: Option<String>,
    pub default: Option<Expr>,
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
    pub visibility: Option<String>,
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
        type_args: Vec<String>,
    },
    New {
        class_name: String,
        args: Vec<Expr>,
    },
    Cast {
        value_type: String,
        argument: Box<Expr>,
        kind: String,
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
    Index {
        object: Box<Expr>,
        index: Box<Expr>,
    },
    OptionalChain {
        object: Box<Expr>,
        name: String,
        args: Option<Vec<Expr>>,
    },
    Coalesce {
        left: Box<Expr>,
        right: Box<Expr>,
    },
    Ternary {
        cond: Box<Expr>,
        then_expr: Box<Expr>,
        else_expr: Box<Expr>,
    },
    /// RFC 0012 try operator: `expr?` unwraps Result or early-returns Err.
    Try {
        argument: Box<Expr>,
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
    Continue,
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
    DoWhile {
        body: Vec<Stmt>,
        test: Expr,
    },
    Try {
        body: Vec<Stmt>,
        err_name: String,
        catch: Vec<Stmt>,
    },
    Delay {
        time: Expr,
        body: Vec<Stmt>,
    },
    Defer {
        body: Vec<Stmt>,
    },
    /// Compile-time-only block (Intent Phase A). Erased from Luau emit.
    Comptime {
        body: Vec<Stmt>,
        span: Span,
    },
    FieldDestructure {
        names: Vec<String>,
        value: Expr,
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
    pub no_banner: bool,
    pub strict_receiver: bool,
    pub release: bool,
    pub defines: Vec<String>,
}

/// Member/call access. `Janitor` is `~>` (signal connect tracked by Janitor).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AccessKind {
    Dot,
    Colon,
    Scope,
    Janitor,
}

impl AccessKind {
    pub fn parse(access: &str) -> Self {
        match access {
            "~>" => Self::Janitor,
            ":" => Self::Colon,
            "::" => Self::Scope,
            _ => Self::Dot,
        }
    }

    pub fn is_janitor(self) -> bool {
        matches!(self, Self::Janitor)
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Dot => ".",
            Self::Colon => ":",
            Self::Scope => "::",
            Self::Janitor => "~>",
        }
    }
}

/// Attach `///` comments that sit immediately above a declaration.
pub fn attach_docs(program: &mut Program, comments: &[SourceComment]) {
    for item in &mut program.items {
        let line = match item {
            Item::Function(f) | Item::Proto(f) => f.line,
            Item::Decl(d) => d.line,
            Item::Class { line, .. } | Item::Enum { line, .. } | Item::TypeAlias { line, .. } => {
                *line
            }
            _ => continue,
        };
        let doc = comments.iter().rev().find(|c| c.is_doc && (c.line + 1 == line || c.line == line));
        let Some(doc) = doc else { continue };
        match item {
            Item::Function(f) | Item::Proto(f) => f.doc = Some(doc.text.clone()),
            Item::Decl(d) => d.doc = Some(doc.text.clone()),
            Item::Class { doc: slot, .. }
            | Item::Enum { doc: slot, .. }
            | Item::TypeAlias { doc: slot, .. } => *slot = Some(doc.text.clone()),
            _ => {}
        }
    }
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
            Stmt::ForEach { span, .. } | Stmt::Comptime { span, .. } => *span,
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

    pub fn access_kind(access: &str) -> AccessKind {
        AccessKind::parse(access)
    }
}
