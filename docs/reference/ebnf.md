# Grammar (EBNF)

Reference grammar for the parser (`src/core/parser/parser.cpp`). The guide explains each construct with examples; this page gives its formal shape. Literals and tokens are described in [Lexer](lexer.md).

```ebnf
program        = { top_item } ;
top_item       = link_decl | struct_decl | enum_decl | variant_decl | type_decl
               | func_decl | namespace_decl | signal_decl | statement ;

(* ---------- modules ---------- *)
link_decl      = ( 'link' | 'import' ) , module , [ 'as' , identifier ] , ';' ;
                 (* without `as`: the last path component with a capital initial *)
module         = '@' , identifier , { '.' , identifier } | string_literal ;
using_decl     = 'using' , identifier , '.' , identifier , ';' ;

(* ---------- declarations ---------- *)
struct_decl    = [ 'abstract' | 'final' ] , 'struct' , identifier , [ type_params ]
                 , [ ':' , type_name , { ',' , type_name } ]
                 , '{' , { field | method } , '}' ;
field          = [ 'private' | 'public' ] , type , identifier , ';' ;
method         = [ 'private' | 'abstract' | 'override' | 'final' ] , func_decl ;
enum_decl      = 'enum' , identifier , '{' , [ enum_case , { ',' , enum_case } ] , '}' ;
enum_case      = identifier , [ '(' , type , ')' ] ;
variant_decl   = 'variant' , identifier , '{' , type , { ',' , type } , '}' ;
type_decl      = 'type' , identifier , '=' , type_expr , ';' ;
type_expr      = type_atom , { ( '|' | '&' ) , type_atom } ;
namespace_decl = 'namespace' , identifier , '{' , { func_decl } , '}' ;
signal_decl    = 'signal' , identifier , ';' ;

func_decl      = [ 'static' | 'async' | 'extern' ] , 'func' , func_name , [ type_params ]
                 , '(' , [ param , { ',' , param } ] , ')' , [ '->' , type ]
                 , [ 'where' , identifier , ':' , type ] , ( block | ';' )
               | 'void' , identifier , '(' , [ param , { ',' , param } ] , ')' , block ;
func_name      = identifier | 'operator' , ( '+' | '-' ) ;
param          = [ type ] , [ '...' ] , identifier , [ '=' , expr ] ;
type_params    = '<' , identifier , { ',' , identifier } , '>' ;

type           = type_atom ;
type_atom      = 'void' | string_literal
               | ( builtin_type | identifier , { '.' , identifier } ) , [ '<' , type , { ',' , type } , '>' ] ;
                 (* Combat.Fighter: the module name is accepted and ignored *)
builtin_type   = 'int' | 'float' | 'double' | 'bool' | 'string' | 'task' | 'buffer'
               | 'Vector2' | 'Vector3' | 'Vector4' ;

(* ---------- statements ---------- *)
statement      = let_stmt | typed_let | const_stmt | assign_stmt | place_assign | self_assign
               | if_stmt | while_stmt | for_stmt | for_in_stmt | switch_stmt | match_stmt
               | try_stmt | return_stmt | 'break' , ';' | 'continue' , ';'
               | connect_stmt | output_stmt | attribute , statement | using_decl | expr , ';' ;

let_stmt       = 'let' , [ 'mut' ] , ( identifier | '(' , identifier , { ',' , identifier } , ')' )
                 , [ ':' , type ] , '=' , expr , ';' ;
typed_let      = ( type | 'auto' | 'decltype' , '(' , identifier , ')' | 'observable' , [ type ] | 'atomic' )
                 , identifier , '=' , expr , ';' ;
const_stmt     = ( 'const' | 'constexpr' ) , identifier , '=' , expr , ';' ;
assign_stmt    = identifier , ( '=' | '+=' | '-=' | '*=' | '/=' | '%=' ) , expr , ';'
               | identifier , ( '++' | '--' ) , ';' ;
place_assign   = identifier , place_step , { place_step } , ( '=' | '+=' | '-=' | '*=' | '/=' | '%=' ) , expr , ';' ;
place_step     = '.' , identifier | '[' , expr , ']' ;
                 (* p.stats.hp -= 3;  xs[i] = v;  team[1].hp = 9;  self.hp = 0; *)
self_assign    = '@' , [ 'this' , ( '.' | '::' ) ] , identifier , '=' , expr , ';' ;

if_stmt        = 'if' , '(' , ( expr | 'let' , identifier , '=' , expr ) , ')' , block
                 , [ 'else' , ( if_stmt | block ) ] ;
while_stmt     = 'while' , '(' , expr , ')' , block ;
for_stmt       = 'for' , '(' , ( let_stmt | assign_stmt ) , expr , ';' , assign_no_semi , ')' , block ;
for_in_stmt    = 'for' , '(' , 'let' , [ 'mut' ] , identifier , 'in' , expr , ')' , block ;
                 (* number: 0..n-1; list/array: elements; text: characters; dictionary: keys *)
switch_stmt    = 'switch' , '(' , expr , ')' , '{' , { ( 'case' , expr | 'default' ) , ':' , { statement } } , '}' ;
match_stmt     = 'match' , '(' , expr , ')' , '{' , { match_arm } , '}' ;
match_arm      = pattern , [ 'guard' , expr ] , '~>' , ( statement | block ) ;
pattern        = '_' | number | string_literal | identifier , [ '(' , identifier , ')' ] ;
try_stmt       = 'try' , block , 'catch' , [ '(' , identifier , ')' ] , block ;
return_stmt    = 'return' , [ expr ] , ';' ;
connect_stmt   = identifier , '~>' , identifier , { '.' , identifier } , ';' ;
output_stmt    = ( 'post' | 'warn' | 'report' | 'cout' ) , '(' , expr , ')' , ';' ;
attribute      = '[[' , identifier , ']]' ;
block          = '{' , { statement } , '}' ;

(* ---------- expressions (lowest to highest precedence) ---------- *)
expr           = or_expr , [ '?' , expr , ':' , expr ] ;
or_expr        = and_expr , { ( 'or' | '||' ) , and_expr } ;
and_expr       = bit_or , { ( 'and' | '&&' ) , bit_or } ;
bit_or         = bit_xor , { '|' , bit_xor } ;
bit_xor        = bit_and , { '^' , bit_and } ;
bit_and        = shift , { '&' , shift } ;
shift          = unary , { ( 'shl' | 'shr' ) , unary } ;
unary          = ( '-' | '~' | 'not' | '!' | 'spawn' | 'await' | 'new' | 'thread' ) , unary | comparison ;
comparison     = additive , { ( '==' | '!=' | '<' | '<=' | '>' | '>=' ) , additive } ;
additive       = multiplicative , { ( '+' | '-' | '.:' | '..' ) , multiplicative } ;
multiplicative = postfix , { ( '*' | '/' | '%' ) , postfix } ;
postfix        = primary , { '(' , [ args ] , ')' | '.' , identifier | '[' , expr , ']' | '++' | '--' } ;
args           = arg , { ',' , arg } ;
arg            = [ identifier , ':' ] , expr ;          (* named argument *)
primary        = number | string_literal | template | 'true' | 'false' | 'null'
               | identifier , [ '<' , type , { ',' , type } , '>' ]
               | builtin_type | '@' , [ 'this' , ( '.' | '::' ) ] , identifier
               | '(' , expr , ')' | '(' , identifier , '=>' , expr , ')'
               | 'func' , '(' , [ param , { ',' , param } ] , ')' , block
               | 'parallel' , '(' , args , ')' | 'pcall' , '(' , expr , ')'
               | 'move' , '(' , identifier , ')' | 'join' , '(' , expr , ')'
               | identifier , '::' , identifier ;     (* buffer::create, string::trim *)
template       = '`' , { text | '${' , expr , '}' } , '`' ;
```

## Notes

- Every block uses braces; there is no brace-free `if`.
- `++` and `--` as statements change the variable; in an expression they return the postfix value.
- An `(x => expr)` lambda can only be called where it is written.
- An imported module contains declarations and `const NAME = literal;` only; see [Modules](../guide/10-modules.md).
