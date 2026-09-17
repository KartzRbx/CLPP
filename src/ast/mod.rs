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
    Ident(String),
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
        iter: Expr,
        body: Vec<Stmt>,
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
    pub is_script: bool,
    pub is_header: bool,
    pub script_kind: Option<String>,
    pub libraries: Vec<String>,
    pub requires: Vec<ModuleRequire>,
}
