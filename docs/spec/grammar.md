# Grammar (subset)

EBNF of the slice the lexer/parser must accept. This is not ISO C++.

```
translation-unit = { include | pragma | using-skip | declaration } ;

include = "#include" ( "<" path ">" | '"' path '"' ) ;
pragma  = "#pragma" ( "once" | "strict" | "nostrict" | "nstrict" | "nonstrict" | "native" | "optimize" [ 0 | 1 | 2 ] ) ;

declaration =
    struct-decl
  | function-def
  | method-def
  | var-decl
  | const-decl
  ;

struct-decl = ( "struct" | "class" ) ident "{" { struct-member } "}" ";" ;
struct-member =
    access-label
  | "static" "constexpr" type ident "=" expr ";"
  | type ident [ "=" expr ] ";"
  | type ident "(" [ param-list ] ")" ";"
  ;

function-def = type ident "(" [ param-list ] ")" block ;
method-def   = type ident "::" ident "(" [ param-list ] ")" block ;

param-list = param { "," param } ;
param      = type ident ;

type =
    "void" | "int" | "float" | "double" | "bool" | "string" | "func" | "auto"
  | ident [ "*" ]
  | ( "LuaArray" | "vector" | "array" | "span" | "optional" ) "<" type ">"
  | "dictionary" "<" type "," type ">"
  | "const" type
  ;

block = "{" { statement } "}" ;

statement =
    var-decl
  | expr ";"
  | "return" [ expr ] ";"
  | "break" ";"
  | if-stmt
  | while-stmt
  | c-for
  | range-for
  | spawn-stmt
  | parallel-stmt
  | guard-stmt
  | match-stmt
  | switch-stmt
  | destructure
  | block
  ;

guard-stmt   = "guard" "(" expr ")" "else" statement ;
spawn-stmt   = "spawn" block [ ";" ] ;
parallel-stmt = "parallel" block [ ";" ] ;
match-stmt   = "match" "(" expr ")" "{" { match-arm } "}" [ ";" ] ;
match-arm    = ( "_" | type ident ) "=>" ( block | expr ) [ "," ] ;
destructure  = "auto" "[" ident { "," ident } "]" "=" expr ";" ;

if-stmt     = "if" "(" expr ")" statement [ "else" statement ] ;
while-stmt  = "while" "(" expr ")" statement ;
c-for       = "for" "(" [ var-init ] ";" [ expr ] ";" [ expr ] ")" statement ;
range-for   = "for" "(" type ident ":" expr ")" statement ;
switch-stmt = "switch" "(" expr ")" "{" { case-clause } "}" ;
case-clause = ( "case" expr | "default" ) ":" { statement } ;

expr =
    ident | number | string | "true" | "false" | "null"
  | expr ( "+" | "-" | "*" | "/" | "==" | "!=" | "<" | ">" | "<=" | ">=" | "&&" | "||" | ".:" ) expr
  | "!" expr | "-" expr
  | expr ( "=" | "+=" | "-=" | "*=" | "/=" ) expr
  | expr "++" | expr "--"
  | expr "::" ident [ "(" arg-list ")" ]
  | expr ":" ident [ "(" arg-list ")" ]
  | expr "." ident [ "(" arg-list ")" ]
  | ident "(" arg-list ")"
  | "new" ident "(" [ arg-list ] ")"
  | "GetService" "<" ident ">" "(" ")"
  | "static_cast" "<" type ">" "(" expr ")"
  | ident "{" { "." ident "=" expr "," } "}"
  | "{" expr { "," expr } "}"
  | "{" "{" expr "," expr "}" { "," "{" expr "," expr "}" } "}"
  | lambda
  | "(" expr ")"
  ;

lambda = [ "func" ] "[]" [ "(" [ param-list ] ")" ] block ;
```

Concatenation uses `.:` (not `..`). `::` is method/scope; `:` is table; `.` is property.

Extra mapped tokens, not pure grammar: `string_concat`, `to_string`, `to_number`, `to_bool`, `cout`, `cerr`, `endl`, `post`, `warn`, `report`.
