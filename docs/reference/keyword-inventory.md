# Lexer keyword inventory

Every reserved keyword in `src/core/lexer/lexer.cpp` has an entry below.
The documentation coverage test compares this table with that file.
Non-keyword contextual names (`any`, `array`, `dictionary`, `optional`, `tuple`,
`connection`, `actor`, `coroutine`, `strict`, `nostrict`) are explained in the
[language contract](language-contract.md); being an identifier does not make them
user-defined APIs. Legacy reserved words deliberately have no executable example.

| Keyword | Status | Meaning and rules |
| --- | --- | --- |
| `Vector2` | current | [Built-in vector/byte-buffer value API](../guide/09-collections-and-text.md) |
| `Vector3` | current | [Built-in vector/byte-buffer value API](../guide/09-collections-and-text.md) |
| `Vector4` | current | [Built-in vector/byte-buffer value API](../guide/09-collections-and-text.md) |
| `abstract` | current | [Data/member inheritance or visibility modifier](../guide/07-structs-and-inheritance.md) |
| `and` | current | [Expression operator or explicit checked cast](../guide/04-operators.md) |
| `as` | current | [Source/built-in module import or exported declaration](../guide/10-modules.md) |
| `async` | current | [Task, worker or synchronization form](../guide/12-concurrency.md) |
| `atomic` | current | [Task, worker or synchronization form](../guide/12-concurrency.md) |
| `auto` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `await` | current | [Task, worker or synchronization form](../guide/12-concurrency.md) |
| `bool` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `break` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `buffer` | current | [Built-in vector/byte-buffer value API](../guide/09-collections-and-text.md) |
| `case` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `catch` | current | [Output, protected evaluation or runtime failure handling](../guide/11-errors.md) |
| `class` | current | [Scoped enum component or literal value](../guide/08-enums-variants-option-result.md) |
| `const` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `constexpr` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `continue` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `cout` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `decltype` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `def` | current | [Data/alias, callback or generic declaration component](../guide/06-functions.md) |
| `default` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `do` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `else` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `endl` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `enum` | current | [Scoped enum component or literal value](../guide/08-enums-variants-option-result.md) |
| `export` | current | [Source/built-in module import or exported declaration](../guide/10-modules.md) |
| `extern` | current | [Restricted external, local move or data operator form](language-contract.md#12-less-common-forms-and-limits) |
| `false` | current | [Scoped enum component or literal value](../guide/08-enums-variants-option-result.md) |
| `final` | current | [Data/member inheritance or visibility modifier](../guide/07-structs-and-inheritance.md) |
| `float` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `for` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `from` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `func` | current | [Data/alias, callback or generic declaration component](../guide/06-functions.md) |
| `guard` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `if` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `import` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `in` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `join` | current | [Task, worker or synchronization form](../guide/12-concurrency.md) |
| `link` | current | [Source/built-in module import or exported declaration](../guide/10-modules.md) |
| `match` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `move` | current | [Restricted external, local move or data operator form](language-contract.md#12-less-common-forms-and-limits) |
| `namespace` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `new` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `not` | current | [Expression operator or explicit checked cast](../guide/04-operators.md) |
| `null` | current | [Scoped enum component or literal value](../guide/08-enums-variants-option-result.md) |
| `number` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `observable` | current | [Observed state or typed notification](../guide/13-events.md) |
| `operator` | current | [Restricted external, local move or data operator form](language-contract.md#12-less-common-forms-and-limits) |
| `or` | current | [Expression operator or explicit checked cast](../guide/04-operators.md) |
| `override` | current | [Data/member inheritance or visibility modifier](../guide/07-structs-and-inheritance.md) |
| `parallel` | current | [Task, worker or synchronization form](../guide/12-concurrency.md) |
| `pcall` | current | [Output, protected evaluation or runtime failure handling](../guide/11-errors.md) |
| `post` | current | [Output, protected evaluation or runtime failure handling](../guide/11-errors.md) |
| `pragma` | current | [File checking directive](comments.md) |
| `private` | current | [Data/member inheritance or visibility modifier](../guide/07-structs-and-inheritance.md) |
| `public` | current | [Data/member inheritance or visibility modifier](../guide/07-structs-and-inheritance.md) |
| `report` | current | [Output, protected evaluation or runtime failure handling](../guide/11-errors.md) |
| `return` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `shl` | current | [Expression operator or explicit checked cast](../guide/04-operators.md) |
| `shr` | current | [Expression operator or explicit checked cast](../guide/04-operators.md) |
| `signal` | current | [Observed state or typed notification](../guide/13-events.md) |
| `spawn` | current | [Task, worker or synchronization form](../guide/12-concurrency.md) |
| `static` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `static_cast` | current | [Expression operator or explicit checked cast](../guide/04-operators.md) |
| `string` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `struct` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `switch` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |
| `task` | current | [Task, worker or synchronization form](../guide/12-concurrency.md) |
| `template` | current | [Data/alias, callback or generic declaration component](../guide/06-functions.md) |
| `thread` | current | [Task, worker or synchronization form](../guide/12-concurrency.md) |
| `true` | current | [Scoped enum component or literal value](../guide/08-enums-variants-option-result.md) |
| `try` | current | [Output, protected evaluation or runtime failure handling](../guide/11-errors.md) |
| `type` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `typename` | current | [Data/alias, callback or generic declaration component](../guide/06-functions.md) |
| `using` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `variant` | removed | Reserved for rejection; see [removed forms](language-contract.md#13-removed-spellings-and-future-proposals) |
| `void` | current | [Primitive/result type or initialized declaration](../guide/03-variables-and-types.md) |
| `warn` | current | [Output, protected evaluation or runtime failure handling](../guide/11-errors.md) |
| `where` | current | [Data/alias, callback or generic declaration component](../guide/06-functions.md) |
| `while` | current | [Control-flow instruction, arm or iteration word](../guide/05-control-flow.md) |


`class` is current only as part of `enum class`, not as an independent class
declaration. `extern` has only the supported strlen signature. `operator` does
not imply arbitrary operator dispatch; see the restricted forms in the contract.
Internal token enumeration comments and historical regression docs are not
additional source-language promises.
