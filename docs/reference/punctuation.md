# Punctuation and source structure

CL++ uses punctuation to make boundaries explicit. Indentation improves readability; it does not define the grammar.

## Delimiters

| Symbol | Role | Example |
| --- | --- | --- |
| `;` | End an instruction or alias | `number hp = 100;` |
| `{ }` | Block, data definition, or initializer | `if (hp > 0) { post(hp); }` |
| `( )` | Arguments, conditions, grouping | `post(hp);`, `(a + b) * c` |
| `[ ]` | Index a collection | `scores[0]` |
| `,` | Separate parameters or values | `Damage(20, 5)` |
| `< >` | Generic type arguments | `array<number>` |

Every opener needs a matching closer. Instructions inside a block still need semicolons. A control-flow block ends with `}`; it does not need an extra semicolon.

## Access and type notation

| Symbol | Meaning in target syntax |
| --- | --- |
| `.` | Member or module access: `hero.Hp`; initializer fields have no dot |
| `::` | Scope access, such as `Element::Fire` |
| `:` | Inheritance after a type name |
| `.:` | Concatenate values into text |
| `->` | Return type in a callback type or typed lambda |
| `=>` | A match arm |
| `~>` | Managed subscription: `event~>Connect(callback)`; `event::Clear()` releases managed handlers |
| `? :` | Choose between two values |
| `@` | Built-in module prefix; contextual annotations need their own contract |

```clp
def Logger = (string) -> void;
def Fighter { string Name; number Hp = 100; }
Fighter hero = Fighter { Name = "Ada" };
post(hero.Name .: ": " .: hero.Hp);
```

```output
Ada: 100
```

Here the `->` belongs to a function type; it is not a member-access operator. `.:` is a single operator, not a dot followed by a type annotation.

## Statements and expressions

An expression computes a value: `hp - 25`, `hero.Hp`, or `Damage(20, 5)`. A statement performs a step: `hp -= 25;`, `return hp;`, or an `if` block. A declaration introduces a name and its type. The entry module executes top-level statements in order; imported modules organize reusable declarations.

## A module has no header

Use `.clp` files and `link` to organize a project. There is no separate header, declaration file, or include phase. Read [Modules](../guide/10-modules.md) for file paths, aliases, and API boundaries.
