# Grammar (EBNF)

This page summarizes current core forms. EBNF abstracts lexical detail and semantic checks; it is not a machine-generated parser grammar. The [language contract](language-contract.md) completes this summary with declarations, concurrency, events, restricted forms, precedence and executable examples. Future proposals remain separate in [implementation status](status.md).

In EBNF, `A = B ;` defines a rule, `|` means alternatives, `[ ... ]` is optional, `{ ... }` repeats zero or more times, and quoted text is literal source. An unquoted grammar semicolon ends a rule; `';'` means an actual semicolon in CL++.

## Files, definitions, and functions

```ebnf
program          = [ directive ], { top_item } ;
directive        = 'pragma', ( 'strict' | 'nostrict' ), ';' ;
top_item         = link_decl | exported_decl | def_decl | enum_decl | function_decl | extern_decl | statement ;
exported_decl    = 'export', ( def_decl | enum_decl | function_decl | const_decl ) ;
link_decl        = 'link', module, [ 'as', identifier ], ';' ;
module           = string_literal | '@', identifier, { '.', identifier } ;

def_decl         = [ 'abstract' | 'final' ], 'def', identifier, [ type_params ],
                   ( [ ':', type, { ',', type } ], '{', { field | method_decl }, '}'
                   | '=', type_expr, ';' ) ;
method_decl      = [ 'public' | 'private' ], [ 'override' ], [ 'final' ], function_decl
                 | 'abstract', [ template_decl ], [ 'async' ], type, identifier,
                   '(', [ parameters ], ')', [ where_clause ], ';' ;
field            = [ 'public' | 'private' ], type, identifier, [ '=', expression ], ';' ;
type_params      = '<', identifier, { ',', identifier }, '>' ;
type_expr        = type, { ( '|' | '&' ), type }
                 | '(', [ type_list ], ')', '->', type ;
type_list        = type, { ',', type } ;
type             = 'void' | string_literal
                 | type_name, [ '<', type_list, '>' ] ;
type_name        = identifier, { ( '.' | '::' ), identifier } ;

enum_decl        = 'enum', 'class', identifier,
                   '{', identifier, { ',', identifier }, '}' ;
function_decl    = [ template_decl ], [ 'async' ], type, identifier, [ '::', identifier ],
                   '(', [ parameters ], ')', [ where_clause ], block ;
extern_decl      = 'extern', 'number', 'strlen', '(', 'string', identifier, ')', ';' ;
template_decl    = 'template', '<', template_param, { ',', template_param }, '>' ;
template_param   = 'typename', identifier, [ ':', type_expr ] ;
where_clause     = 'where', identifier, ':', type_expr, { ',', identifier, ':', type_expr } ;
parameters       = type, identifier, { ',', type, identifier } ;
lambda           = [ 'async' ], 'func', '(', [ lambda_params ], ')', '->', type, block ;
lambda_params    = [ type ], identifier, { ',', [ type ], identifier } ;
```

`def Name { ... }` declares data; `def Name = ...;` aliases a type, union, or callback. Named functions put the result first; callbacks use `->`. Field defaults belong to the definition. There are no headers, default function arguments, or function overloading in this specification.

An omitted lambda parameter type requires an unambiguous expected callback signature. Async callbacks return `task<T>` at invocation; their body result annotation denotes `T`. See [concurrency](../guide/12-concurrency.md#async-callbacks-and-captured-state) for current scheduling rules.

## Variables and control flow

Typed notifications use `signal<T> name;` (or multiple payload types, including `signal<>`). `event::Connect(callback)` and `event::Once(callback)` return `connection` handles. `event~>Connect(callback)` registers with the event's internal cleanup manager; `event::Clear()` disconnects its managed subscriptions. `event::Fire(arguments)` invokes typed synchronous handlers in registration order, stopping on the first error. See [events](../guide/13-events.md).

```ebnf
statement        = variable_decl | const_decl | assignment | if_stmt | while_stmt | do_stmt
                 | for_stmt | foreach_stmt | switch_stmt | match_stmt | guard_stmt | try_stmt
                 | return_stmt | 'break', ';' | 'continue', ';' | expression, ';' ;
variable_decl    = ( type | 'auto' ), identifier, '=', expression, ';' ;
const_decl       = ( 'const' | 'constexpr' ), ( type | 'auto' ), identifier, '=', expression, ';' ;
assignment       = place, assign_operator, expression, ';'
                 | place, ( '++' | '--' ), ';' ;
assign_operator  = '=' | '+=' | '-=' | '*=' | '/=' | '%=' ;
place            = identifier, { '.', identifier | '[', expression, ']' } ;
if_stmt          = 'if', '(', expression, ')', block,
                   [ 'else', ( if_stmt | block ) ] ;
while_stmt       = 'while', '(', expression, ')', block ;
do_stmt          = 'do', block, 'while', '(', expression, ')', ';' ;
for_stmt         = 'for', '(', variable_decl, expression, ';', step, ')', block ;
step             = place, ( '++' | '--' ) | place, assign_operator, expression ;
foreach_stmt     = 'for', '(', ( type | 'auto' ), identifier, 'in', expression, ')', block ;
switch_stmt      = 'switch', '(', expression, ')', '{', { switch_arm }, '}' ;
switch_arm       = ( 'case', expression | 'default' ), ':', { statement } ;
match_stmt       = 'match', '(', expression, ')', '{', { match_arm }, '}' ;
match_arm        = pattern, [ 'guard', expression ], '=>', ( statement | block ) ;
pattern          = scoped_name | number | string_literal | '_' ;
scoped_name      = identifier, { '::', identifier } ;
guard_stmt       = 'guard', '(', expression, ')', 'else', block ;
try_stmt         = 'try', block, 'catch', [ '(', identifier, ')' ], block ;
return_stmt      = 'return', [ expression ], ';' ;
block            = '{', { statement }, '}' ;
```

A guard's failure block exits the current path (`return`, `continue`, or another appropriate exit). Match exhaustiveness and optional narrowing are semantic checks, not just punctuation rules. See [Control flow](../guide/05-control-flow.md).

## Expressions and initialization

```ebnf
expression       = primary
                 | unary_operator, expression
                 | expression, binary_operator, expression
                 | expression, '?', expression, ':', expression ;
primary          = number | string_literal | template | 'true' | 'false' | 'null'
                 | '@this'
                 | '(', type, expression, ')'
                 | scoped_name | lambda | '(', expression, ')'
                 | primary, '(', [ arguments ], ')'
                 | primary, '.', identifier
                 | primary, '[', expression, ']'
                 | type, '{', [ field_initializers ], '}'
                 | '{', [ arguments ], [ ',' ], '}'
                 | 'static_cast', '<', type, '>', '(', expression, ')' ;
arguments        = expression, { ',', expression } ;
field_initializers = identifier, '=', expression,
                     { ',', identifier, '=', expression } ;
unary_operator   = '-' | 'not' | '!' | '~' ;
binary_operator  = '+' | '-' | '*' | '/' | '%' | '.:' | '..'
                 | '==' | '!=' | '<' | '<=' | '>' | '>='
                 | 'and' | '&&' | 'or' | '||'
                 | '&' | '|' | '^' | 'shl' | 'shr' ;
template         = '`', { text | '${', expression, '}' }, '`' ;
```

The expression rules summarize forms rather than encode precedence. The current parser's exact precedence is listed in the [language contract](language-contract.md#3-expressions-access-and-precedence) and checked by compiler tests. Prefix grouping is conventional; comparisons sharing a level and binding before shifts are explicit current rules.

## What this summary leaves to topic pages

The following additional forms are part of the current language; the linked
contract supplies their semantic rules and runnable examples:

```ebnf
decltype_decl    = 'decltype', '(', expression, ')', identifier, '=', expression, ';' ;
atomic_decl      = 'atomic', identifier, '=', expression, ';' ;
observable_decl  = 'observable', ( type | 'auto' ), identifier, '=', expression, ';' ;
signal_decl      = 'signal', '<', [ type_list ], '>', identifier, [ '=', expression ], ';' ;
event_call       = expression, '::', identifier, '(', [ arguments ], ')' ;
managed_connect  = expression, '~>', 'Connect', '(', expression, ')' ;
async_prefix     = ( 'await' | 'spawn' | 'thread' ), expression ;
protected_call   = 'pcall', '(', expression, ')' ;
move_expr        = 'move', '(', identifier, ')' ;
join_expr        = 'join', '(', expression, ')' ;
parallel_expr    = 'parallel', '(', [ arguments ], ')' ;
operator_name    = 'operator', ( '+' | '-' ) ;
dynamic_variadic = 'any', '...', identifier ;
```

An operator name occurs in the name position of a typed function declaration.
The existing dynamic variadic parameter is a restricted packing form, not a
general type-level variadic/spread facility. Contextual APIs such as `actor`,
`coroutine`, `mutex`, collection constructors and built-in modules are documented
as operations rather than additional grammar productions. Parser recognition of
an internal token does not override the documented runtime restrictions.

The [lexer](lexer.md) defines names, literals, whitespace, and comments. [Types](../guide/08-enums-variants-option-result.md), [Functions](../guide/06-functions.md), and [Modules](../guide/10-modules.md) define meaning and visibility. [Concurrency](../guide/12-concurrency.md) and [Events](../guide/13-events.md) describe the tested scheduling and subscription contracts. [Future proposals](future-proposals.md) remain separate from implemented features.
