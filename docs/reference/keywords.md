# Keywords and operators

Use this page to find a language word by purpose. The [complete lexer keyword inventory](keyword-inventory.md) includes every reserved word, with removed forms identified. The [language contract](language-contract.md) also covers contextual type names, less common forms and semantics. See [Implementation status](status.md).

## Declaration words

| Writing | Meaning | Explanation |
| --- | --- | --- |
| `number`, `float`, `bool`, `string`, `void` | Primitive or return type | [Variables](../guide/03-variables-and-types.md) |
| `auto` | Infer a variable type | [Variables](../guide/03-variables-and-types.md) |
| `const` | Fixed value with an explicit or inferred type | [Variables](../guide/03-variables-and-types.md) |
| `def` | Data definition or type alias | [Definitions](def.md) |
| `enum class` | Scoped named cases | [Enums](../guide/08-enums-variants-option-result.md) |
| `optional` | May contain a value or be null | [Optional values](../guide/08-enums-variants-option-result.md) |
| `func` | Anonymous callback value | [Functions](../guide/06-functions.md) |
| `template`, `typename` | Generic function parameters | [Functions](../guide/06-functions.md) |
| `where` | Additional generic type constraints | [Functions](../guide/06-functions.md) |
| `abstract`, `override`, `final`, `public`, `private` | Inheritance contracts and member access | [Inheritance](../guide/07-structs-and-inheritance.md) |
| `link`, `as` | Import a module and name its alias | [Modules](../guide/10-modules.md) |
| `export` | Expose a declaration to module callers | [Modules](../guide/10-modules.md) |
| `pragma strict;` | Request strict checking | [Directives](comments.md) |

## Control flow

| Writing | Meaning |
| --- | --- |
| `if`, `else` | Choose a branch |
| `while` | Test a condition before each iteration |
| `do` / `while` | Run the body first, then test the condition |
| `for`, `in` | Counter loop or typed collection iteration |
| `break`, `continue` | Leave a loop or advance its iteration |
| `return` | Exit a function with an optional value |
| `match` | Choose an exhaustive case using `=>` arms |
| `guard`, `else` | Require a condition; exit on failure |
| `try`, `catch` | Recover from a runtime failure |

[Control flow](../guide/05-control-flow.md) explains each form independently. [Errors](../guide/11-errors.md) distinguishes compile-time diagnostics from runtime failures.

## Expressions

| Category | Spelling |
| --- | --- |
| Values | `true`, `false`, `null` |
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| Assignment | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `++`, `--` |
| Comparison | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Logic | `and` / `&&`, `or` / `\|\|`, `not` / `!` |
| Bits | `&`, `\|`, `^`, `~`, `shl`, `shr` |
| Display text | `.:`, backtick `${expression}` templates |
| Selection | `condition ? yes : no` |
| Access | `object.Member`, `Element::Case`, `collection[index]` |
| Callback result | `->` in a callback type or lambda |
| Cast | `static_cast<Type>(value)` |

[Operators](../guide/04-operators.md) explains meaning; [Punctuation](punctuation.md) explains boundaries. Operator precedence and explicit numeric cast checking are compiler-tested.

## Runtime extensions and APIs

`signal`, `connection`, `observable` and typed `tuple` event payloads are explained in [Events](../guide/13-events.md). `async`, `await`, `spawn`, `task`, `thread`, `join`, `parallel`, and atomic operations are explained in [Concurrency](../guide/12-concurrency.md), with tested scheduling and synchronization rules.

`post`, `warn`, `report`, `len`, `list`, `push`, `pop`, and the collection constructors are runtime callables. They are not user-defined logging aliases or replacements for declaration syntax. The [library](../guide/14-standard-library.md) documents their existing APIs.

Access modifiers, restricted data operators, the existing dynamic variadic form and current pragma modes are described in the [language contract](language-contract.md). Their implementation does not imply arbitrary C++ modifiers, custom operators, variadic generics or new checking modes.
