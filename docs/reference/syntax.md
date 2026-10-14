# Syntax map

This is the language reference index. Each topic has its own explanation; the standard library describes APIs rather than the grammar of CL++.

The author-defined syntax is primary. [Implementation status](status.md) lists what still needs compiler work. Examples labeled target syntax are specifications, not evidence that the current executable accepts them.

## Complete language handbook

[Start with the language's purpose](../handbook/00-o-que-e-o-clpp.md), then read each topic separately. [def and types](../handbook/07-def-e-tipos.md) brings the type forms together, and the [handbook reference](../handbook/14-referencia.md) includes a glossary and syntax summary.

## Writing a file

| Topic | Rules and examples |
| --- | --- |
| First program and reading code | [Learn CL++ from zero](../guide/00-language-tour.md) |
| Source characters and tokens | [Lexer](lexer.md) |
| Comments and file directives | [Comments](comments.md) |
| Names, capitalization, reserved words | [Identifiers](identifiers.md), [Keywords](keywords.md) |
| Numbers, strings, templates, null | [Literals](literals.md) |
| Braces, parentheses, semicolons, separators | [Punctuation](punctuation.md) |

## Values and expressions

| Topic | Rules and examples |
| --- | --- |
| Typed declarations, inference, const | [Variables and types](../guide/03-variables-and-types.md) |
| Arithmetic, comparisons, logic, bits | [Operators](../guide/04-operators.md) |
| Lists, arrays, dictionaries, indexing | [Collections and text](../guide/09-collections-and-text.md) |
| Data definitions, fields, inheritance | [Data definitions](../guide/07-structs-and-inheritance.md) |
| Enums, literal unions, optional values | [Advanced types](../guide/08-enums-variants-option-result.md) |

## Instructions and reusable behavior

| Topic | Rules and examples |
| --- | --- |
| if, while, for, break, continue | [Control flow](../guide/05-control-flow.md) |
| match, cases, guards, narrowing | [Control flow](../guide/05-control-flow.md), [Advanced types](../guide/08-enums-variants-option-result.md) |
| Typed functions, parameters, return | [Functions](../guide/06-functions.md) |
| Typed callbacks, lambdas, generics | [Functions](../guide/06-functions.md) |
| Imports and visibility without headers | [Modules](../guide/10-modules.md) |
| Exceptions and diagnostics | [Errors](../guide/11-errors.md) |
| Async work, tasks, synchronization | [Concurrency](../guide/12-concurrency.md) |
| Events and observed state | [Events](../guide/13-events.md) |

## Exact forms and tooling

[Reference grammar](ebnf.md) · [Keyword index](keywords.md) · [Toolchain](toolchain.md) · [Editor](../guide/15-tools-and-editor.md)

A word being recognized by an old lexer does not make it part of the author-defined language. Compiler compatibility and target syntax are listed separately in [Implementation status](status.md).
