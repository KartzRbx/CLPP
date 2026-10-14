# Learn CL++ from zero

CL++ is a language with its own writing rules. Start here before the standard library. You do not need to know C++ or Luau.

The site describes the language defined by its author and implemented in the current
checkout. Target blocks identify future proposals, native effects or application
contracts that are not executable examples. See [Implementation status](../reference/status.md)
and [Future proposals](../reference/future-proposals.md).

## 1. A source file is a module

Create a file named `hello.clp`. A module is a file that groups related code. The entry file contains the instructions that start your program. There are no headers, include files, or mandatory `main()` function.

```clp
post("Hello, CL++!");
```

`post` displays a value. Parentheses contain its argument. Quotes delimit text. The semicolon ends the instruction. Run `clpp hello.clp` from a terminal, or use Ctrl+F5 in VS Code.

## 2. Name a value and choose its type

```clp
const number MAX_HP = 100;
number hp = MAX_HP;
string name = "Ada";
hp -= 25;
post(name .: ": " .: hp .: " HP");
```

```output
Ada: 75 HP
```

Read `number hp = MAX_HP;` as: create an integer named `hp`, initialized with `MAX_HP`. A typed variable can change. `const` makes a value fixed. `.:` joins values into display text. `auto` requests inference instead of writing a type. See [Variables and types](03-variables-and-types.md) for the full declaration rules and implementation status.

## 3. Make a decision

```clp
number hp = 75;
if (hp > 0) {
    post("alive");
} else {
    post("defeated");
}
```

The condition is between parentheses; each branch is a block between braces. `>` compares values. Indentation helps people read the code; braces determine the block.

## 4. Give behavior a name

```clp
number Damage(number attack, number defense) {
    number amount = attack - defense;
    return amount > 1 ? amount : 1;
}
post(Damage(20, 5));
```

```output
15
```

The first `number` is the returned type. `Damage` is the function name. Each parameter has a type and a name. `return` sends a value back to the caller. The conditional expression `condition ? yes : no` chooses one value. See [Functions](06-functions.md).

## 5. Group your game data

```clp
def Fighter {
    string Name;
    number Hp = 100;
}
Fighter hero = Fighter { Name = "Ada" };
post(hero.Name .: ": " .: hero.Hp);
```

```output
Ada: 100
```

`def` defines a type. Its fields describe one fighter. The initializer fills named fields; omitted fields use declared defaults. A dot reads a member. See [Data definitions and inheritance](07-structs-and-inheritance.md).

## 6. Continue by topic

| Question | Lesson |
| --- | --- |
| How are characters read? | [Lexer](../reference/lexer.md) |
| How do I write notes? | [Comments and directives](../reference/comments.md) |
| How do I name things? | [Identifiers](../reference/identifiers.md) |
| How do I write numbers and text? | [Literals](../reference/literals.md) |
| What do all the symbols mean? | [Punctuation](../reference/punctuation.md) |
| How do calculations work? | [Operators](04-operators.md) |
| How do I repeat or branch? | [Control flow](05-control-flow.md) |
| How do I model states and missing values? | [Enums, unions, and optionals](08-enums-variants-option-result.md) |
| How do I store many values? | [Collections](09-collections-and-text.md) |
| How do I split a project? | [Modules](10-modules.md) |
| How do I understand errors? | [Errors and diagnostics](11-errors.md) |
| How do I coordinate work and reactions? | [Concurrency](12-concurrency.md), [Events](13-events.md) |

Use the [syntax map](../reference/syntax.md) to find a specific construct. Learn the language first; visit the [library](14-standard-library.md) when you need an API.
