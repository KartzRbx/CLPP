# Gramática (EBNF)

Gramática de referência do parser (`src/core/parser/parser.cpp`). O guia explica cada construção com exemplos; aqui está a forma exata. Literais e tokens estão em [lexer.md](lexer.md).

```ebnf
program        = { top_item } ;
top_item       = link_decl | struct_decl | enum_decl | variant_decl | type_decl
               | func_decl | namespace_decl | signal_decl | statement ;

(* ---------- módulos ---------- *)
link_decl      = ( 'link' | 'import' ) , module , [ 'as' , identifier ] , ';' ;
                 (* sem `as`: o último nome do caminho com inicial maiúscula *)
module         = '@' , identifier , { '.' , identifier } | string_literal ;
using_decl     = 'using' , identifier , '.' , identifier , ';' ;

(* ---------- declarações ---------- *)
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
                 (* Combat.Fighter: o nome do módulo é aceito e ignorado *)
builtin_type   = 'int' | 'float' | 'double' | 'bool' | 'string' | 'task' | 'buffer'
               | 'Vector2' | 'Vector3' | 'Vector4' ;

(* ---------- comandos ---------- *)
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
                 (* número: 0..n-1; lista/array: elementos; texto: caracteres; dicionário: chaves *)
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

(* ---------- expressões (da menor para a maior precedência) ---------- *)
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
arg            = [ identifier , ':' ] , expr ;          (* argumento nomeado *)
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

## Notas

- Todo bloco usa chaves; não existe `if` sem `{ }`.
- `++`/`--` como comando alteram a variável; dentro de expressão devolvem o valor (pós-fixo).
- Uma lambda `(x => expr)` só pode ser chamada no lugar em que é escrita.
- Um módulo importado só pode conter declarações e `const NOME = literal;` (ver o [guia, capítulo 10](guia/10-modulos.md)).
