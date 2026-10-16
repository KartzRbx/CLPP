# 9. Collections and text

Collections hold multiple values. Indices start at zero. `len` reports the number of elements, dictionary entries, or Unicode characters. An invalid index raises a runtime error.

## Lists and indexed access

`list(...)` makes a growable sequence. `auto` infers a declaration's type; target declarations are mutable unless marked `const`. `push` adds an element; `items[index]` reads one.

```clp
array<string> items = array<string>("sword", "shield");
push(items, "potion");
number index = 0;
while (index < len(items)) {
  post(items[index]);
  index += 1;
}
```
```output
sword
shield
potion
```

## Typed arrays

`array<T>` restricts element types. Repeat `<T>` on the constructor. The constructor checks its elements and preserves `T` for inferred variables, calls and returns. Mutable generic values keep compatible type arguments in both directions: `array<number>` cannot be assigned to `array<float>` or `array<string>`. Convert elements into a new collection when changing their type.

```clp
array<number> scores = array<number>(10, 20, 30);
post(scores[1]);
post(len(scores));
```
```output
20
3
```

## Iterating a collection

Use a type or `auto` followed by the element name. Arrays yield elements, dictionaries yield keys, strings yield Unicode characters, and a `number` count yields integers from zero up to the count, excluding it. Decimal counts are rejected.

```clp
array<number> scores = array<number>(10, 20, 30);
for (number score in scores) {
  post(score);
}
```

```output
10
20
30
```

An inferred loop element retains its real type, including when the collection comes from a function, an alias or a generic argument. Property checking and editor completion use that same type.

```clp
def Player { number Health; }
array<Player> MakeTeam() {
  return array<Player>(Player { Health = 42 });
}
for (auto player in MakeTeam()) {
  post(player.Health);
}
```
```output
42
```

## Dictionaries

`dictionary<K, V>` maps keys of type `K` to values of type `V`. Constructor arguments alternate key and value. Index with a key of type `K`. Reading or removing a missing key raises an error; successful access returns `V`. Assigning to a missing key adds an entry.

```clp
dictionary<string, number> stock = dictionary<string, number>("potion", 3, "shield", 1);
post(stock["potion"]);
post(len(stock));
```
```output
3
2
```

## Mutation and ownership

`push` and `insert` accept the array element type and return `void`. `pop` and `remove` return the removed value. Positions must be `number` values. `find` returns a zero-based position, or `-1` when absent. `sort` returns an array preserving its numeric element type and exact integer values; it rejects `NaN`.

Arrays, dictionaries and `def` objects retain shared identity through assignments and parameters. `==` compares that identity. Applying `const` freezes the reachable data contents through every alias. `push`, `pop`, `insert` and `remove` also accept collection fields, indexed collections and collection-returning expressions. New slices and sorted arrays have their own container identity; their object elements still refer to the original objects.

### Const through aliases

```clp
def Player { number Health = 100; }
auto player = Player {};
auto team = array<Player>(player);
const auto fixed = team;
try { player.Health = 20; } catch (error) { post("frozen"); }
post(fixed[0].Health);
```
```output
frozen
100
```

The heap traces cycles without recursively destroying object chains. Diagnostic output prints `<cycle>` for a cyclic edge and limits excessive nesting. JSON serialization rejects cycles instead of inventing a JSON tree. Synchronize compound operations explicitly when data is shared across threads.

## Integer ranges

`start .. end` creates an `array<number>` including the start and excluding the end. Values preserve 64-bit integer precision. An unrepresentable length raises an error.

```clp
post(3 .. 6);
```
```output
[3, 4, 5]
```

## Strings and slices

Strings contain UTF-8 text. Indexing uses Unicode characters. `text[start .. end]` includes the start and excludes the end. Slice bounds are integers and are clamped to the collection boundaries; arrays also support slices that preserve their element type. `.:` concatenates text with another value.

```clp
string text = "abcd";
post(text[1 .. 3]);
post("Score: " .: 12);
```
```output
bc
Score: 12
```

See [lexical syntax](02-lexical-syntax.md) for string delimiters and interpolation. Import `@clpp.text` for additional text operations.

## Bytes and vectors

The current engine supplies `buffer::create(size)` for byte buffers and `Vector2`, `Vector3`, and `Vector4` for mathematical coordinates. These runtime APIs are separate from language declaration syntax. Use strings for text, buffers for binary bytes, and typed arrays for ordinary sequences.
