#!/usr/bin/env node
/**
 ** Writes docs/reference/*.md — one page per language utility,
 * in the same spirit as https://cplusplus.com/reference/
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const OUT = path.join(ROOT, "docs", "reference");
fs.mkdirSync(OUT, { recursive: true });

function heading(title) {
  if (/[<>{}]/.test(title)) {
    return `# \`${title.replace(/`/g, "")}\``;
  }
  return `# ${title}`;
}

function page({
  id,
  title,
  sidebar,
  header,
  summary,
  syntax,
  params,
  returns,
  emit,
  description,
  exampleClpp,
  exampleLuau,
  notes,
  see,
}) {
  const paramTable = params?.length
    ? [
        "## Parameters",
        "",
        "| Name | Type | Description |",
        "| --- | --- | --- |",
        ...params.map((p) => `| \`${p.name}\` | \`${p.type}\` | ${p.desc} |`),
        "",
      ].join("\n")
    : "## Parameters\n\nNone.\n";

  const seeLine = (see || [])
    .map((s) => {
      const [label, href] = Array.isArray(s) ? s : [s, s];
      return `[${label}](${href})`;
    })
    .join(" · ");

  const notesBlock = notes ? `## Notes\n\n${notes}\n\n` : "";

  const example = exampleClpp
    ? `## Example\n\n\`\`\`clpp\n${exampleClpp}\n\`\`\`\n${
        exampleLuau ? `\nEmits:\n\n\`\`\`luau\n${exampleLuau}\n\`\`\`\n` : ""
      }`
    : "";

  return `---
title: ${JSON.stringify(title)}
sidebar_label: ${JSON.stringify(sidebar || title)}
---

${heading(title)}

<div class="clpp-ref-meta">${header || "Language"}</div>

${summary}

## Syntax

\`\`\`clpp
${syntax}
\`\`\`

${paramTable}
## Return value

${returns}

## Luau emit

\`${emit}\`

## Description

${description}

${notesBlock}${example}
## See also

${seeLine || "—"}
`;
}

const pages = [];

function add(p) {
  pages.push(p);
}

/* ───────── I/O ───────── */
add({
  id: "post",
  title: "post",
  header: "I/O · builtin",
  summary:
    "Writes values to the output log. This is CL++'s standard print — the analog of C `printf` / C++ `std::cout` / Luau `print`.",
  syntax: "void post(...);",
  params: [{ name: "...", type: "any", desc: "Values to print, separated by commas." }],
  returns: "`void`. Nothing is returned.",
  emit: "print(...)",
  description: `Every argument is forwarded to Luau \`print\`. There is no format string. Concatenate text with [\`.:\`](operator-concat) before printing, or pass several arguments.

Prefer \`post\` over legacy [\`cout\`](cout).`,
  exampleClpp: `post("hello");
post("coins:", 100);
post("online: " .: players.GetPlayers());`,
  exampleLuau: `print("hello")
print("coins:", 100)
print("online: " .. players:GetPlayers())`,
  see: ["warn", "report", "cout", "operator-concat"],
});

add({
  id: "warn",
  title: "warn",
  header: "I/O · builtin",
  summary:
    "Writes a yellow warning. Same name and meaning as Luau `warn`. Use for recoverable problems.",
  syntax: "void warn(...);",
  params: [{ name: "...", type: "any", desc: "Warning payload." }],
  returns: "`void`.",
  emit: "warn(...)",
  description: `Does not stop the thread. Pair with [\`guard\`](guard) when the failure is expected and you \`return\` afterwards. Use [\`report\`](report) when the program cannot continue.`,
  exampleClpp: `guard (player != null) else {
    warn("Invalid player");
    return;
}`,
  exampleLuau: `if not (player ~= nil) then
	warn("Invalid player")
	return
end`,
  see: ["post", "report", "guard"],
});

add({
  id: "report",
  title: "report",
  header: "I/O · builtin",
  summary:
    "Throws. This is CL++'s analog of C++ `throw` / Luau `error`. The current thread stops.",
  syntax: "void report(...);",
  params: [{ name: "...", type: "any", desc: "Error payload (usually a string)." }],
  returns: "Does not return.",
  emit: "error(...)",
  description: `There is no \`try/catch\` in CL++. Use [\`pcall\`](pcall) when you need to catch a failure. \`report\` is for unrecoverable states.`,
  exampleClpp: `if (coins < 0) {
    report("Balance sync error.");
}`,
  exampleLuau: `if coins < 0 then
	error("Balance sync error.")
end`,
  see: ["post", "warn", "pcall"],
});

add({
  id: "cout",
  title: "cout / endl",
  sidebar: "cout",
  header: "I/O · legacy stream",
  summary:
    "C++ iostream leftover. Parses, but `post` is the language's real print.",
  syntax: `cout << arg << arg << endl;`,
  params: [
    { name: "arg", type: "any", desc: "Each `<<` is another print argument." },
    { name: "endl", type: "manipulator", desc: "Ends the line." },
  ],
  returns: "`void`.",
  emit: "print(arg, arg)",
  description: `Each \`<<\` becomes another argument to \`print\`. \`endl\` finishes the statement. There is no \`cin\`, \`scanf\`, or stream formatting (\`std::setw\`). Game input comes from Instances, DataStores, and signals.`,
  exampleClpp: `cout << "hi" << endl;`,
  exampleLuau: `print("hi")`,
  notes: "New code should use [`post`](post).",
  see: ["post"],
});

/* ───────── builtins ───────── */
add({
  id: "GetService",
  title: "GetService",
  header: "Builtin · Roblox",
  summary:
    "Looks up a Roblox service by type name. The only generic besides collections and `static_cast`.",
  syntax: "T GetService<T>();",
  params: [{ name: "T", type: "class name", desc: "Service class, e.g. `Players`, `RunService`." }],
  returns: "An Instance of class `T` (`game:GetService(\"T\")`).",
  emit: 'game:GetService("T")',
  description: `\`T\` is the Roblox class name, not a C++ template you define. You must \`#include <clpp/roblox.clh>\` (IntelliSense) so the editor knows the type.

There is no \`game:GetService\` in CL++ source — always this form.`,
  exampleClpp: `#include <clpp/roblox.clh>

void init() {
    Players players = GetService<Players>();
    post("online: " .: players.GetPlayers());
}`,
  exampleLuau: `local players: Players = game:GetService("Players")
print("online: " .. players:GetPlayers())`,
  see: ["new", "include", "instance-pointer"],
});

add({
  id: "pcall",
  title: "pcall",
  header: "Builtin · error handling",
  summary:
    "Protected call. CL++ has no `try/catch`; this is how you catch [`report`](report) / Luau `error`.",
  syntax: "auto [ok, result] = pcall(fn);",
  params: [{ name: "fn", type: "func", desc: "Callback to run. Usually `func (…) { }`." }],
  returns: "Multiple values: success flag, then the result or the error message.",
  emit: "pcall(fn)",
  description: `Pair with [destructuring](destructure). If \`ok\` is false, \`result\` is the error string. There is also Luau \`xpcall\` if you include it as a global — the compiler treats it as a call.`,
  exampleClpp: `auto [success, result] = pcall(func () {
    return DataStore.GetAsync("PlayerData");
});`,
  exampleLuau: `local success, result = pcall(function()
	return DataStore:GetAsync("PlayerData")
end)`,
  see: ["report", "destructure", "lambda"],
});

add({
  id: "new",
  title: "new",
  header: "Builtin · constructor",
  summary:
    "Constructs a Roblox Instance or a library object. Not C++ heap allocation — there is no `delete`.",
  syntax: `auto child = new Class(parent);
auto janitor = new Janitor();`,
  params: [
    { name: "Class", type: "identifier", desc: "Instance class or lib type (`Janitor`)." },
    { name: "parent", type: "Instance?", desc: "First argument becomes `.Parent` on Instances." },
  ],
  returns: "The constructed object.",
  emit: 'Instance.new("Class") / Class.new()',
  description: `**Instances** (\`Folder\`, \`IntValue\`, \`Part\`, …): emit \`Instance.new("Class")\`. The first argument is assigned to \`.Parent\`.

**Libraries** (\`Janitor\` and similar): emit \`Janitor.new()\`.

**Datatypes** (\`Vector3\`, \`CFrame\`, \`UDim2\`, \`Color3\`): do **not** use \`new\`. Call them as values: \`Vector3(8, 1, 8)\`.`,
  exampleClpp: `auto coins = new IntValue(leaderstats);
auto janitor = new Janitor();
part.Size = Vector3(8, 1, 8);`,
  exampleLuau: `local coins: IntValue = Instance.new("IntValue")
coins.Parent = leaderstats
local janitor = Janitor.new()
part.Size = Vector3.new(8, 1, 8)`,
  see: ["GetService", "instance-pointer", "observable"],
});

add({
  id: "static_cast",
  title: "static_cast",
  header: "Builtin · cast",
  summary:
    "Source-level type annotation. Luau has no runtime casts — this does not check `ClassName`.",
  syntax: "T static_cast<T>(value);",
  params: [
    { name: "T", type: "type", desc: "Target type, often `Folder` / `Player`." },
    { name: "value", type: "any", desc: "Expression to re-annotate." },
  ],
  returns: "`value` unchanged, annotated as `T`.",
  emit: "value  (with a Luau type annotation)",
  description: `\`const_cast\`, \`reinterpret_cast\`, and \`dynamic_cast\` also emit the argument. \`(void)x;\` is dropped (silences unused in clangd).

To branch on Instance class at runtime, use [\`match\`](match) (\`IsA\`).`,
  exampleClpp: `Folder folder = static_cast<Folder>(existingFolder);`,
  exampleLuau: `local folder: Folder = existingFolder`,
  see: ["match", "instance-pointer", "auto"],
});

add({
  id: "string_concat",
  title: "string_concat",
  header: "Builtin · strings",
  summary:
    "Variadic string join. Prefer the [`.:`](operator-concat) operator for two operands.",
  syntax: "string string_concat(...);",
  params: [{ name: "...", type: "any", desc: "Values coerced and joined with Luau `..`." }],
  returns: "`string`. Zero args → `\"\"`. One arg → that value.",
  emit: "(a .. b .. c)",
  description: `Valid even when you mix numbers. Binary concatenation is still [\`.:\`](operator-concat). Do **not** write Lua \`..\`, and do **not** use \`+\` on strings.`,
  exampleClpp: `return string_concat(player.Name, "_", "LeaderstatsJanitor");
return player.Name .: "_" .: "LeaderstatsJanitor";`,
  exampleLuau: `return (player.Name .. "_" .. "LeaderstatsJanitor")
return (player.Name .. "_") .. "LeaderstatsJanitor"`,
  see: ["operator-concat", "string"],
});

add({
  id: "null",
  title: "null",
  header: "Literal",
  summary:
    "Absence of a value. `nullptr` is accepted as a synonym. Emits Luau `nil`.",
  syntax: `Player ref = null;
if (player != null) { }`,
  params: [],
  returns: "The `nil` value.",
  emit: "nil",
  description: `There is no C \`NULL\` macro. Compare with [\`!=\`](operator-comparison). [\`guard (player != null)\`](guard) is the usual early-out.

Uninitialized locals (\`int coins;\`) also emit \`nil\` — always initialize.`,
  exampleClpp: `Player playerRef = null;
guard (playerRef != null) else {
    return;
}`,
  exampleLuau: `local playerRef: Player = nil
if not (playerRef ~= nil) then
	return
end`,
  see: ["optional", "guard", "instance-pointer"],
});

add({
  id: "init",
  title: "void init()",
  sidebar: "init",
  header: "Script entry",
  summary:
    "Script / LocalScript entry point. CL++ has no `int main()`.",
  syntax: `void init() {
    // ...
}`,
  params: [],
  returns: "`void`.",
  emit: "init()  (called at the end of the file)",
  description: `Only Scripts and LocalScripts run \`init()\` at the bottom of the emitted file. Untagged \`.clpp\` files are ModuleScripts: they \`return\` the table of \`Class::\` methods and must **not** rely on \`init\` as a Roblox entry.

Construct **one** service object in \`init()\` and close over it from lambdas — that is the game singleton.

In \`init()\`, a synthetic \`__janitor\` is created and also \`game:BindToClose\`.`,
  exampleClpp: `#include <clpp/roblox.clh>

void init() {
    Players players = GetService<Players>();
    post("ready");
}`,
  exampleLuau: `local players: Players = game:GetService("Players")
print("ready")
init()`,
  see: ["struct", "lambda", "operator-janitor"],
});

add({
  id: "this",
  title: "@this",
  sidebar: "@this",
  header: "OOP",
  summary:
    "The current object inside `Class::Method`. Emits Luau `self`. `@field` is that object's member. `this` (no `@`) is the same alias.",
  syntax: `void Service::Tick() {
    @janitor.Cleanup();
    @this;
    this.janitor.Add(conn);
    coins = coins + 1; // bare field → self.coins
}`,
  params: [],
  returns: "The receiver table.",
  emit: "self · self.field",
  description: `Only valid inside [\`Class::Method\`](class-method). A free function or \`void init()\` that uses \`@this\` / \`@field\` is an error.

\`@this\` is a value: pass it to other functions (\`other.Register(@this)\` → \`other:Register(self)\`). There is no \`this->\` and no \`@this\` parameter on the signature — \`::\` already injects the receiver.`,
  exampleClpp: `void CombatServer::BindPart(BasePart part) {
    @janitor.Add(part, "Destroy");
    other.Register(@this);
}`,
  exampleLuau: `function CombatServer:BindPart(part: BasePart)
	self.janitor:Add(part, "Destroy")
	other:Register(self)
end`,
  see: ["struct", "class-method"],
});

/* ───────── types ───────── */
function typePage(id, title, emit, summary, extra, exampleClpp, exampleLuau, see) {
  add({
    id,
    title,
    header: "Type",
    summary,
    syntax: `${title} name = value;`,
    params: [],
    returns: `A value of type \`${title}\`, emitted as \`${emit}\`.`,
    emit,
    description: extra,
    exampleClpp,
    exampleLuau,
    see,
  });
}

typePage(
  "int",
  "int",
  "number",
  "32-bit-looking integer in source. Luau numbers are IEEE-754 doubles — `int` is a documentation type.",
  `Use for counts, coins, user ids stored as numbers. [\`observable int\`](observable) becomes \`IntValue\`.

There is no \`int8_t\` / \`size_t\` distinct emit — they are not special.`,
  `int coins = 100;
coins += 1;`,
  `local coins: number = 100
coins += 1`,
  ["float", "double", "observable"]
);

typePage(
  "float",
  "float",
  "number",
  "Floating-point number. Same Luau emit as `int` and `double`.",
  `[\`observable float\`](observable) becomes \`NumberValue\`. Use for speed, alpha, damage with fractions.`,
  `float speed = 16.5;`,
  `local speed: number = 16.5`,
  ["int", "double"]
);

typePage(
  "double",
  "double",
  "number",
  "Same emit as `float`. Write `double` when you want the C++ name.",
  `[\`observable double\`](observable) becomes \`NumberValue\`.`,
  `double alpha = 0.25;`,
  `local alpha: number = 0.25`,
  ["int", "float"]
);

typePage(
  "bool",
  "bool",
  "boolean",
  "Boolean. Literals are `true` and `false`.",
  `[\`observable bool\`](observable) becomes \`BoolValue\`. \`!\` emits \`not\`. There is no \`YES\`/\`NO\`.`,
  `bool isActive = true;
if (!isActive) {
    return;
}`,
  `local isActive: boolean = true
if not isActive then
	return
end`,
  ["operator-logic", "observable"]
);

typePage(
  "string",
  "string",
  "string",
  "UTF-8 text. Not `std::string`. Concatenate with [`.:`](operator-concat).",
  `[\`observable string\`](observable) becomes \`StringValue\`. Double quotes only in the current grammar. There is no string_view.`,
  `string name = "Kartz";
string key = name .: "_LeaderstatsJanitor";`,
  `local name: string = "Kartz"
local key: string = name .. "_LeaderstatsJanitor"`,
  ["operator-concat", "string_concat"]
);

add({
  id: "void",
  title: "void",
  header: "Type",
  summary: "No value. Return type of procedures. `void init()` is the script entry.",
  syntax: `void Name(T arg) {
    return;
}`,
  params: [],
  returns: "Nothing. Luau omits the return annotation (or uses `()` internally).",
  emit: "(no return annotation)",
  description: `\`return;\` is valid. \`return expr;\` in a \`void\` function should be avoided. \`void\` is only a return type.`,
  exampleClpp: `void CreateLeaderstats(Player player) {
    return;
}`,
  exampleLuau: `const function CreateLeaderstats(player: Player)
	return
end`,
  see: ["init", "function"],
});

typePage(
  "func",
  "func",
  "(...any) -> any",
  "Function type and anonymous callback prefix. Callbacks, listeners, `pcall` bodies.",
  `There are no C++ captures. Write \`func (int n) { }\` or assign \`func cb = func() {};\`.`,
  `func onCoinsChanged = func (int newValue) {
    post("New value: " .: newValue);
};`,
  `local onCoinsChanged: (...any) -> any = function(newValue: number)
	print("New value: " .. newValue)
end`,
  ["lambda", "function", "Connect"]
);

add({
  id: "auto",
  title: "auto",
  header: "Type",
  summary:
    "Infer the type from the initializer. Prefer explicit types on parameters and struct fields.",
  syntax: `auto players = GetService<Players>();
auto janitor = new Janitor();
auto [ok, result] = pcall(fn);`,
  params: [],
  returns: "Whatever the initializer produces.",
  emit: "local name = …  (Luau annotation when known)",
  description: `Inferred for \`new Class(...)\`, [\`GetService<T>()\`](GetService), datatype constructors, and destructuring.

Do not use \`auto\` as a replacement for a public API type.`,
  exampleClpp: `auto coins = new IntValue(leaderstats);`,
  exampleLuau: `local coins: IntValue = Instance.new("IntValue")
coins.Parent = leaderstats`,
  see: ["new", "GetService", "destructure"],
});

add({
  id: "optional",
  title: "optional<T>",
  sidebar: "optional",
  header: "Type",
  summary: "A value that may be missing. Emits Luau `T?`.",
  syntax: "optional<T> name = null;",
  params: [{ name: "T", type: "type", desc: "Inner type." }],
  returns: "`T` or [`null`](null).",
  emit: "T?",
  description: `Not \`std::optional\` with \`.value()\`. Test with \`!= null\` or [\`guard\`](guard).`,
  exampleClpp: `optional<Player> target = null;`,
  exampleLuau: `local target: Player? = nil`,
  see: ["null", "guard"],
});

add({
  id: "array",
  title: "array<T>",
  sidebar: "array",
  header: "Collection",
  summary:
    "Ordered list. Also spelled `vector<T>`, `LuaArray<T>`, `span<T>`. Emits a Luau array table `{T}`.",
  syntax: `array<T> name = { a, b, c };`,
  params: [{ name: "T", type: "type", desc: "Element type." }],
  returns: "A list.",
  emit: "{T}",
  description: `Brace lists are arrays. Iterate with [range-for](range-for). There is no \`std::vector::push_back\` — use Luau table APIs (\`table.insert\`) via \`::\` if you wrap them, or index.

\`map<K,V>\` is **not** this — see [\`dictionary\`](dictionary).`,
  exampleClpp: `array<string> names = {"Kartz", "Player1"};
for (string n in names) {
    post(n);
}`,
  exampleLuau: `local names: {string} = { "Kartz", "Player1" }
for _, n in names do
	print(n)
end`,
  see: ["dictionary", "range-for", "vector"],
});

add({
  id: "vector",
  title: "vector<T>",
  sidebar: "vector",
  header: "Collection",
  summary: "Alias of [`array<T>`](array). Not `Vector3`.",
  syntax: "vector<T> name = { a, b };",
  params: [{ name: "T", type: "type", desc: "Element type." }],
  returns: "A list (`{T}`).",
  emit: "{T}",
  description: `\`Vector3\` is a Roblox datatype (a value). \`vector<int>\` is a list of numbers. Do not confuse them.`,
  exampleClpp: `vector<int> scores = {1, 2, 3};`,
  exampleLuau: `local scores: {number} = { 1, 2, 3 }`,
  see: ["array", "dictionary"],
});

add({
  id: "dictionary",
  title: "dictionary<K, V>",
  sidebar: "dictionary",
  header: "Collection",
  summary:
    "String-keyed (or typed-key) map. `map<K,V>` is the same emit. Access keys with [`:`](operator-table), not `.`.",
  syntax: `dictionary<K, V> name = {
    {"Key", value},
    {"Other", value2}
};`,
  params: [
    { name: "K", type: "type", desc: "Key type (usually `string`)." },
    { name: "V", type: "type", desc: "Value type." },
  ],
  returns: "A map table.",
  emit: "{ [K]: V }",
  description: `Initializer entries are \`{"Key", value}\` pairs, emitted as \`Key = value\` when the key is a string.

Read/write: \`stats.Coins\` → \`stats.Coins\`. Instance properties use the same \`.\`.`,
  exampleClpp: `dictionary<string, int> stats = {
    {"Coins", 100},
    {"Gems", 50}
};
post(stats.Coins);`,
  exampleLuau: `local stats: { [string]: number } = { Coins = 100, Gems = 50 }
print(stats.Coins)`,
  see: ["array", "operator-table"],
});

add({
  id: "instance-pointer",
  title: "Instance types",
  sidebar: "Instance",
  header: "Type",
  summary:
    "Write the Roblox class name. `Player player` is an Instance of class Player. There is no address-of, no `->`, and no C pointer type.",
  syntax: `Player player = null;
player.Name = "Kartz";
player.FindFirstChild("leaderstats");`,
  params: [],
  returns: "The Instance.",
  emit: "Player",
  description: `Properties and instance methods use [\`.\`](operator-property). Protected calls use [\`:\`](operator-table). Static names use [\`::\`](operator-method). Lifetime is Roblox's: \`Destroy\` or Janitor.

[\`match\`](match) arms use the same class name (\`Part p\`).

[\`observable\`](observable) of a non-primitive becomes \`ObjectValue\`.`,
  exampleClpp: `player.Name = "Kartz";
player.FindFirstChild("leaderstats");`,
  exampleLuau: `player.Name = "Kartz"
player:FindFirstChild("leaderstats")`,
  see: ["new", "operator-method", "operator-property", "null"],
});

add({
  id: "const",
  title: "const / constexpr",
  sidebar: "const",
  header: "Type qualifier",
  summary: "`const` and `static constexpr` become Luau `const`.",
  syntax: `const int DoubleCoins(int coins) {
    return coins;
}
static constexpr int CAP = 100;`,
  params: [],
  returns: "A const binding.",
  emit: "const / const function",
  description: `Function declarations emit \`const function\`. File-level constants emit \`const\`. There is no \`mutable\`. \`public:\` / \`private:\` do not affect constness.`,
  exampleClpp: `const int DoubleCoins(int coins) {
    return coins;
}`,
  exampleLuau: `const function DoubleCoins(coins: number): number
	return coins
end`,
  see: ["function", "struct"],
});

add({
  id: "signal-type",
  title: "signal<T...>",
  sidebar: "signal",
  header: "Type",
  summary:
    "Typed BindableEvent. Declaration emits `__signal()`. Fire with `.Fire`; listen with `~>` or `::Connect`.",
  syntax: "signal<Player, int> OnCoinsUpdated;",
  params: [{ name: "T...", type: "types", desc: "Payload types, comma-separated." }],
  returns: "A signal object (`RBXScriptSignal`-like).",
  emit: "__signal()",
  description: `See the operations: [\`Fire\`](Fire), [\`Connect\`](Connect), [\`Once\`](Once), [\`Wait\`](Wait). Prefer [\`~>\`](operator-janitor) so Janitor owns the connection.`,
  exampleClpp: `signal<Player, int> OnCoinsUpdated;
OnCoinsUpdated.Fire(player, 500);`,
  exampleLuau: `local OnCoinsUpdated = __signal()
OnCoinsUpdated:Fire(player, 500)`,
  see: ["Fire", "Connect", "Once", "Wait", "operator-janitor"],
});

add({
  id: "observable",
  title: "observable T",
  sidebar: "observable",
  header: "Type",
  summary:
    "A ValueBase whose identifier reads and writes `.Value`. Assigning fires `Changed`.",
  syntax: `observable T name = value;
name.OnChange(fn);
name = next;`,
  params: [{ name: "T", type: "type", desc: "`int` → IntValue, `float`/`double` → NumberValue, `string` → StringValue, `bool` → BoolValue, else ObjectValue." }],
  returns: "The ValueBase instance.",
  emit: "Instance.new(\"...Value\"); name.Value = …",
  description: `Reading \`name\` in an expression uses \`.Value\`. Writing \`name = x\` writes \`.Value\` and fires \`Changed\`.

[\`.OnChange(fn)\`](OnChange) is \`Changed:Connect(fn)\`.`,
  exampleClpp: `observable int coins = 100;
coins.OnChange(func (int newValue) {
    post("now " .: newValue);
});
coins = 50;
post(coins);`,
  exampleLuau: `local coins: IntValue = Instance.new("IntValue")
coins.Value = 100
coins.Changed:Connect(function(newValue: number)
	print("now " .. newValue)
end)
coins.Value = 50
print(coins.Value)`,
  see: ["OnChange", "int", "new"],
});

/* ───────── operators ───────── */
add({
  id: "operator-method",
  title: ":: (static / manual Connect)",
  sidebar: "::",
  header: "Operator",
  summary:
    "CL++ does **not** use `->`. `::` is static scope, datatype/library names, class method definitions, and **manual** signal connections (you Disconnect; Janitor does not).",
  syntax: `Vector3::new(1, 0, 1);
task::wait(1);
players.PlayerAdded::Connect(fn);
void Class::Method(...) { }`,
  params: [],
  returns: "The call result, or the nested name.",
  emit: "Vector3.new(...) · task.wait(1) · players.PlayerAdded:Connect(fn) · function Class:Method",
  description: `**Static / modules:** \`task::wait\`, \`Vector3::new\`, \`BrickColor::Red()\`.

**Manual connections:** \`players.PlayerAdded::Connect(fn)\` emits \`players.PlayerAdded:Connect(fn)\` with **no** Janitor. You own \`Disconnect\`. Prefer [\`~>\`](operator-janitor) when a janitor is in scope.

**Definitions:** \`Class::Method\` in a \`.clpp\` is the method body (\`function Class:Method\`).

Instance methods on a value use [\`.\`](operator-property): \`player.Kick()\`, \`workspace.FindFirstChild("x")\`. Protected calls use [\`:\`](operator-table).`,
  exampleClpp: `task::wait(1);
players.PlayerAdded::Connect(fn);
player.FindFirstChild("x");
DataService.Server.WaitFor(p);`,
  exampleLuau: `task.wait(1)
players.PlayerAdded:Connect(fn)
player:FindFirstChild("x")
DataService.Server:WaitFor(p)`,
  see: ["operator-property", "operator-table", "operator-janitor", "class-method", "Connect"],
});

add({
  id: "operator-table",
  title: ": (type / protected call)",
  sidebar: ":",
  header: "Operator",
  summary:
    "Two jobs: **types** on names, and **Safe Mode** calls that cannot crash the script.",
  syntax: `age: int = 10;
player:Kick();
auto child = workspace:FindFirstChild("x");`,
  params: [],
  returns: "For a call: the result on success, `nil` on error.",
  emit: "local age: number = 10  /  pcall of the method",
  description: `**Types.** Prefix C++ types still work (\`int age = 10\`). \`:\` is the other spelling: \`age: int = 10\`.

**Protected calls.** \`player:Kick()\` is Luau-style \`:\` **and** a \`pcall\`. Prefer [\`.\`](operator-property) when the call should throw (\`player.Kick()\`).

Bare \`Table:Key\` without \`()\` still emits \`Table.Key\` (old table-key spelling). New code uses \`.\`: \`DataService.Server\`.`,
  exampleClpp: `age: int = 10;
Instance child = workspace:FindFirstChild("Missing");
guard (child != null) else {
    return;
}`,
  exampleLuau: `local age: number = 10
local child: Instance = (function()
	local _ok, _r = pcall(function()
		return workspace:FindFirstChild("Missing")
	end)
	return if _ok then _r else nil
end)()`,
  see: ["pcall", "operator-property", "operator-method", "int"],
});

add({
  id: "operator-property",
  title: ". (property / instance method)",
  sidebar: ".",
  header: "Operator",
  summary:
    "The default accessor. Properties stay `.` in Luau. Instance method calls emit Luau `:`.",
  syntax: `instance.Property
instance.Property = value;
instance.Method(args);`,
  params: [],
  returns: "The property value, or the method result.",
  emit: "instance.Property · instance:Method(args)",
  description: `Use \`.\` for **everything standard**: \`Name\`, \`Parent\`, \`Value\`, dictionary keys, and instance methods (\`Kick\`, \`FindFirstChild\`, \`WaitForChild\`, …).

Protected (non-throwing) calls use [\`:\`](operator-table). Static names and manual \`Connect\` use [\`::\`](operator-method). Janitor connections use [\`~>\`](operator-janitor).

Designated initializers also start with \`.\`: [\`.Field = value\`](operator-designated).`,
  exampleClpp: `player.Name = "Kartz";
player.Kick();
workspace.FindFirstChild("Baseplate");
DataService.Server.WaitFor(player);`,
  exampleLuau: `player.Name = "Kartz"
player:Kick()
workspace:FindFirstChild("Baseplate")
DataService.Server:WaitFor(player)`,
  see: ["operator-table", "operator-method", "operator-janitor", "operator-designated"],
});

add({
  id: "operator-concat",
  title: ".: (concat)",
  sidebar: ".:",
  header: "Operator",
  summary: "String concatenation. Emits Luau `..`. Never use `+` or Lua `..` in CL++.",
  syntax: "a .: b .: c",
  params: [
    { name: "a", type: "string-ish", desc: "Left operand." },
    { name: "b", type: "string-ish", desc: "Right operand." },
  ],
  returns: "A string.",
  emit: "a .. b",
  description: `Associates left-to-right: \`a .: b .: c\` → \`(a .. b) .. c\`. [\`string_concat\`](string_concat) joins many arguments in one \`(..)\` group.

\`+\` only adds numbers.`,
  exampleClpp: `return player.Name .: "_LeaderstatsJanitor";
return player.Name .: "_" .: "LeaderstatsJanitor";`,
  exampleLuau: `return player.Name .. "_LeaderstatsJanitor"
return (player.Name .. "_") .. "LeaderstatsJanitor"`,
  see: ["string_concat", "string", "post"],
});

add({
  id: "operator-janitor",
  title: "~> (janitor Connect / Once)",
  sidebar: "~>",
  header: "Operator",
  summary:
    "Subscribe and give the connection to Janitor. `~>Connect` and `~>Once` only.",
  syntax: `signal~>Connect(fn);
signal~>Once(fn);`,
  params: [{ name: "fn", type: "func", desc: "Listener." }],
  returns: "The connection (also stored on the janitor).",
  emit: 'janitor:Add(signal:Connect(fn), "Disconnect")',
  description: `Looks up \`janitor\` local, \`self.janitor\`, or a synthetic \`__janitor\`. In [\`void init()\`](init), \`__janitor\` also gets \`game:BindToClose\`.

Bare \`::Connect\` does **not** register with Janitor. Prefer \`~>\` in production.

\`Once\` disconnects after the first emission.`,
  exampleClpp: `players.PlayerAdded~>Connect(func (Player player) {
    post("Connected and managed automatically!");
});
players.PlayerAdded~>Once(func (Player player) {
    post("First player only");
});`,
  exampleLuau: `janitor:Add(players.PlayerAdded:Connect(function(player: Player)
	print("Connected and managed automatically!")
end), "Disconnect")
janitor:Add(players.PlayerAdded:Once(function(player: Player)
	print("First player only")
end), "Disconnect")`,
  see: ["Connect", "Once", "Fire", "init", "header-janitor"],
});

add({
  id: "operator-arithmetic",
  title: "+ − * /",
  sidebar: "+ − * /",
  header: "Operator",
  summary: "Numeric arithmetic. `+` does **not** concatenate. `*` is multiply, never pointer deref.",
  syntax: "a + b - c * d / e",
  params: [],
  returns: "A number.",
  emit: "same operators",
  description: `Left-to-right association with usual precedence. Unary \`-\` is negation. Join text with [\`.:\`](operator-concat).`,
  exampleClpp: `int total = coins + gems * 2;`,
  exampleLuau: `local total: number = coins + gems * 2`,
  see: ["operator-assignment", "operator-increment", "operator-concat"],
});

add({
  id: "operator-comparison",
  title: "== != < > <= >=",
  sidebar: "== !=",
  header: "Operator",
  summary: "`!=` is the one that changes: it emits Luau `~=`. The others stay the same.",
  syntax: "a == b\na != b\na < b",
  params: [],
  returns: "`bool`.",
  emit: "==  ~=  <  >  <=  >=",
  description: `There is no \`===\`. \`null\` compares with \`==\` / \`!=\`.`,
  exampleClpp: `if (currentValue.Value != newValue) {
    currentValue.Value = newValue;
}`,
  exampleLuau: `if currentValue.Value ~= newValue then
	currentValue.Value = newValue
end`,
  see: ["operator-logic", "null", "if"],
});

add({
  id: "operator-logic",
  title: "&& || !",
  sidebar: "&& || !",
  header: "Operator",
  summary: "Boolean logic. Emits Luau `and` / `or` / `not`.",
  syntax: "a && b || !c",
  params: [],
  returns: "`bool` (Luau truthiness).",
  emit: "and  or  not",
  description: `There is no ternary \`? :\`. Use \`if\` / \`else\`. Short-circuit matches Luau.`,
  exampleClpp: `if (player != null && player.Parent) {
    post(player.Name);
}`,
  exampleLuau: `if player ~= nil and player.Parent then
	print(player.Name)
end`,
  see: ["operator-comparison", "if", "guard"],
});

add({
  id: "operator-assignment",
  title: "= += −= *= /=",
  sidebar: "=",
  header: "Operator",
  summary: "Assignment. Compound forms emit the same operators.",
  syntax: "name = value;\nname += 1;",
  params: [],
  returns: "The assigned value (statement form).",
  emit: "=  +=  -=  *=  /=",
  description: `[\`observable\`](observable) assignment writes \`.Value\`. There is no copy-assignment operator overload.`,
  exampleClpp: `coins += 10;
name = player.Name;`,
  exampleLuau: `coins += 10
name = player.Name`,
  see: ["operator-increment", "observable"],
});

add({
  id: "operator-increment",
  title: "++ / −−",
  sidebar: "++ −−",
  header: "Operator",
  summary: "Increment / decrement. Always emits `+= 1` / `-= 1` (not a distinct prefix/postfix value).",
  syntax: "i++;\n++i;\ni--;",
  params: [],
  returns: "Used as a statement; the C++ value distinction is not preserved.",
  emit: "i += 1  /  i -= 1",
  description: `Typical in [C-style for](for). Do not rely on \`x = i++\` returning the old value.`,
  exampleClpp: `for (int i = 0; i < 10; i++) {
    post(i);
}`,
  exampleLuau: `local i = 0
while i < 10 do
	print(i)
	i += 1
end`,
  see: ["for", "operator-assignment"],
});

add({
  id: "operator-designated",
  title: ".Field = (designated init)",
  sidebar: ".Field =",
  header: "Operator",
  summary: "C++ designated initializer. Becomes a Luau table field.",
  syntax: `Type {
    .Field = value,
    .Other = value2
}`,
  params: [],
  returns: "A table.",
  emit: "{ Field = value, Other = value2 }",
  description: `Used for option bags (\`DataServiceOptions\`). Not an Instance property write — that is [\`obj.Prop =\`](operator-property).`,
  exampleClpp: `DataService.Server.Init(DataServiceOptions {
    .Template = playerData,
    .StoreName = "PlayerData",
    .UseMock = true
});`,
  exampleLuau: `DataService.Server:Init({
	Template = playerData,
	StoreName = "PlayerData",
	UseMock = true
})`,
  see: ["operator-property", "struct", "operator-table"],
});

/* ───────── control ───────── */
add({
  id: "if",
  title: "if / else if / else",
  sidebar: "if",
  header: "Control flow",
  summary: "C-style branch. `else if` emits Luau `elseif`.",
  syntax: `if (cond) {
} else if (other) {
} else {
}`,
  params: [{ name: "cond", type: "bool", desc: "Parentheses required." }],
  returns: "None (statements).",
  emit: "if / elseif / else / end",
  description: `There is no ternary \`? :\`. There is no \`elif\` spelling — write \`else if\`.`,
  exampleClpp: `if (coins > 50) {
    post("Enough balance!");
} else if (coins == 0) {
    warn("No coins!");
} else {
    report("Balance sync error.");
}`,
  exampleLuau: `if coins > 50 then
	print("Enough balance!")
elseif coins == 0 then
	warn("No coins!")
else
	error("Balance sync error.")
end`,
  see: ["guard", "switch", "match", "operator-logic"],
});

add({
  id: "while",
  title: "while",
  header: "Control flow",
  summary: "Loop while the condition is true. There is no `do/while`.",
  syntax: "while (cond) {\n}",
  params: [{ name: "cond", type: "bool", desc: "Checked before each iteration." }],
  returns: "None.",
  emit: "while … do … end",
  description: `There is no [\`continue\`](../unsupported). Use nested \`if\` or restructure. [\`break\`](break) leaves the loop.`,
  exampleClpp: `while (true) {
    post("tick");
    break;
}`,
  exampleLuau: `while true do
	print("tick")
	break
end`,
  see: ["for", "range-for", "break"],
});

add({
  id: "for",
  title: "for (C-style)",
  sidebar: "for",
  header: "Control flow",
  summary: "C `for`. Emits `while` plus a trailing increment. Semicolons separate clauses.",
  syntax: "for (int i = 0; i < n; i++) {\n}",
  params: [
    { name: "init", type: "stmt", desc: "Runs once." },
    { name: "cond", type: "bool", desc: "Checked every iteration." },
    { name: "step", type: "expr", desc: "Usually `i++`." },
  ],
  returns: "None.",
  emit: "local i = 0 / while i < n do / i += 1",
  description: `The \`:\` of [range-for](range-for) is a different form: \`for (T name in collection)\`. Do not mix them.`,
  exampleClpp: `for (int i = 0; i < 10; i++) {
    post("Count: " .: i);
}`,
  exampleLuau: `local i = 0
while i < 10 do
	print("Count: " .. i)
	i += 1
end`,
  see: ["range-for", "while", "operator-increment"],
});

add({
  id: "range-for",
  title: "for (T x in list)",
  sidebar: "for-each",
  header: "Control flow",
  summary: "Range-for. Emits `for _, x in list`. The `:` here is not a table key.",
  syntax: "for (T x in list) {\n}",
  params: [
    { name: "T", type: "type", desc: "Element type." },
    { name: "x", type: "ident", desc: "Loop variable." },
    { name: "list", type: "iterable", desc: "Array or iterator-producing call." },
  ],
  returns: "None.",
  emit: "for _, x in list do",
  description: `The index is discarded (\`_\`). To get keys, iterate a dictionary in Luau style via a helper, or use C-for on numeric arrays.

Parentheses and braces are required.`,
  exampleClpp: `for (Player player in players.GetPlayers()) {
    post("Player connected: " .: player.Name);
}`,
  exampleLuau: `for _, player in players:GetPlayers() do
	print("Player connected: " .. player.Name)
end`,
  see: ["for", "array", "operator-table"],
});

add({
  id: "switch",
  title: "switch",
  header: "Control flow",
  summary:
    "Evaluates the discriminant **once**. `break` leaves the switch. No C fall-through; stacked `case`s share a body.",
  syntax: `switch (action) {
case "buy":
case "purchase":
    Grant(player);
    break;
default:
    warn("unknown");
    break;
}`,
  params: [{ name: "action", type: "any", desc: "Compared with `==` to each case." }],
  returns: "None.",
  emit: "repeat … until true  wrapping if / elseif",
  description: `Emitted as \`if\` / \`elseif\` / \`else\` inside \`repeat … until true\` so [\`break\`](break) still leaves the switch. Stacked \`case\`s share the body. \`default\` is the fallback.`,
  exampleClpp: `switch (action) {
case "buy":
    Grant(player);
    break;
default:
    warn("unknown");
    break;
}`,
  exampleLuau: `-- discriminant evaluated once, then if/elseif inside repeat-until-true`,
  see: ["match", "if", "break"],
});

add({
  id: "guard",
  title: "guard",
  header: "Control flow",
  summary:
    "If the condition is false, the `else` block runs (usually `return`). Early-out, not an `if` replacement.",
  syntax: `guard (condition) else {
    warn("…");
    return;
}`,
  params: [{ name: "condition", type: "bool", desc: "Must be true to continue." }],
  returns: "None. The else block typically returns.",
  emit: "if not (condition) then … end",
  description: `The \`else\` is **required**. After a successful guard, the compiler (and the reader) treat the condition as established — e.g. \`player != null\`.`,
  exampleClpp: `guard (player != null) else {
    warn("Invalid player");
    return;
}
post(player.Name);`,
  exampleLuau: `if not (player ~= nil) then
	warn("Invalid player")
	return
end
print(player.Name)`,
  see: ["if", "null", "match"],
});

add({
  id: "match",
  title: "match",
  header: "Control flow",
  summary:
    "Type switch. Instance types use `IsA`; primitives use `typeof`. `_` is the fallback.",
  syntax: `match (value) {
    Type name => statement,
    Other o => statement,
    _ => statement
};`,
  params: [{ name: "value", type: "any", desc: "Scrutinee. Evaluated once." }],
  returns: "None (statement).",
  emit: 'if x:IsA("Type") then … elseif typeof(x) == "…" then …',
  description: `Arms are a class or primitive name plus a binding (\`Part p\`, \`string s\`) — not a C++ pointer. Instance arms emit \`IsA("Part")\`. \`_\` is required if the match is not exhaustive in practice — always provide it.

This is not C++ \`std::variant\` visit and not Luau \`if-then-else\` expressions.`,
  exampleClpp: `match (instance) {
    Part p => p.BrickColor = BrickColor::Red(),
    Model m => m.PrimaryPart.BrickColor = BrickColor::Blue(),
    _ => warn("Instance not supported")
};`,
  exampleLuau: `if instance:IsA("Part") then
	local p = instance
	p.BrickColor = BrickColor.Red()
elseif instance:IsA("Model") then
	local m = instance
	m.PrimaryPart.BrickColor = BrickColor.Blue()
else
	warn("Instance not supported")
end`,
  see: ["switch", "guard", "static_cast", "instance-pointer"],
});

add({
  id: "break",
  title: "break",
  header: "Control flow",
  summary: "Leaves the innermost loop or `switch`. There is no `continue`.",
  syntax: "break;",
  params: [],
  returns: "None.",
  emit: "break",
  description: `In [\`switch\`](switch), \`break\` leaves the synthetic \`repeat-until-true\`. There is no labeled break and no \`goto\`.`,
  exampleClpp: `while (true) {
    break;
}`,
  exampleLuau: `while true do
	break
end`,
  see: ["while", "for", "switch", "../unsupported"],
});

add({
  id: "return",
  title: "return",
  header: "Control flow",
  summary: "Leave the current function, optionally with a value.",
  syntax: "return;\nreturn expr;",
  params: [{ name: "expr", type: "T", desc: "Must match the function return type when present." }],
  returns: "Ends the function.",
  emit: "return  /  return expr",
  description: `[\`void\`](void) functions use \`return;\`. Multiple return values use [destructuring](destructure) on the caller side, not \`return a, b\` in current CL++ style — return a table or a single value.`,
  exampleClpp: `const int DoubleCoins(int coins) {
    return coins;
}`,
  exampleLuau: `const function DoubleCoins(coins: number): number
	return coins
end`,
  see: ["function", "void", "guard"],
});

/* ───────── functions / oop ───────── */
add({
  id: "function",
  title: "function",
  header: "Functions",
  summary:
    "Named function. Only bodies in `.clp` / `.clpp` emit. Prototypes in `.clh` become `export type` fields.",
  syntax: `ReturnType Name(T arg) {
    return arg;
}`,
  params: [],
  returns: "Whatever `ReturnType` is.",
  emit: "const function Name(arg: T): ReturnType",
  description: `No overloading. No default arguments. No templates except [\`GetService<T>\`](GetService) / collections / [\`static_cast\`](static_cast).

[\`async\`](async) functions may [\`await\`](await). [\`void init()\`](init) is the script entry.`,
  exampleClpp: `void CreateLeaderstats(Player player) {
    return;
}

const int DoubleCoins(int coins) {
    return coins;
}`,
  exampleLuau: `const function CreateLeaderstats(player: Player)
	return
end

const function DoubleCoins(coins: number): number
	return coins
end`,
  see: ["lambda", "async", "init", "class-method"],
});

add({
  id: "lambda",
  title: "func (...)",
  sidebar: "func (...)",
  header: "Functions",
  summary:
    "Anonymous callback. There is no C++ capture list — CL++ has no pointers to capture. Luau still closes over outer locals.",
  syntax: `func (T arg) { }
func () { }
func cb = func () {};`,
  params: [],
  returns: "A function value.",
  emit: "function(arg: T) … end",
  description: `[\`func\`](func) is both the type and the keyword that starts an inline callback. Passing a method by name from inside \`Class::\` binds \`self\`: \`function(...) self:OnPlayer(...) end\`.

Keep Instances alive with Janitor; closures do not own Roblox lifetime. Do **not** write \`func [](…)\` or \`[]() { }\`.`,
  exampleClpp: `players.PlayerAdded~>Connect(func (Player playerEntered) {
    post("New player: " .: playerEntered.Name);
});`,
  exampleLuau: `janitor:Add(players.PlayerAdded:Connect(function(playerEntered: Player)
	print("New player: " .. playerEntered.Name)
end), "Disconnect")`,
  see: ["func", "Connect", "function", "this"],
});

add({
  id: "async",
  title: "async",
  header: "Concurrency",
  summary: "Marks a function that may `await`. Does not emit a Promise wrapper by itself.",
  syntax: `async T Name(Args args) {
    T x = await expr;
    return x;
}`,
  params: [],
  returns: "`T`.",
  emit: "const function  (body uses __await)",
  description: `[\`await expr\`](await) calls \`__await\`: if the value has \`:expect()\` (Promise), wait; otherwise return it (already yielded).`,
  exampleClpp: `async Data FetchData(Player player) {
    Data data = await DataService.Server.WaitFor(player);
    return data;
}`,
  exampleLuau: `const function FetchData(player: Player): Data
	local data: Data = __await(DataService.Server:WaitFor(player))
	return data
end`,
  see: ["await", "spawn", "function"],
});

add({
  id: "await",
  title: "await",
  header: "Concurrency",
  summary: "Wait for a Promise-like value inside an `async` function.",
  syntax: "T x = await expr;",
  params: [{ name: "expr", type: "any", desc: "Promise (`:expect()`) or already-resolved value." }],
  returns: "The resolved value.",
  emit: "__await(expr)",
  description: `Not JS \`await\` in the event loop sense beyond what Luau Promises provide. Use [\`spawn\`](spawn) to run a block without blocking the caller.`,
  exampleClpp: `Data data = await DataService.Server.WaitFor(player);`,
  exampleLuau: `local data: Data = __await(DataService.Server:WaitFor(player))`,
  see: ["async", "spawn", "pcall"],
});

add({
  id: "spawn",
  title: "spawn",
  header: "Concurrency",
  summary: "Run a block on a new thread. Emits `task.spawn`.",
  syntax: `spawn {
    task::wait(2);
    post("Delay finished!");
};`,
  params: [],
  returns: "None (fires and forgets).",
  emit: "task.spawn(function() … end)",
  description: `The block is a body, not a callback you pass. For Parallel Luau, see [\`parallel\`](parallel).`,
  exampleClpp: `spawn {
    task::wait(2);
    post("Delay finished!");
};`,
  exampleLuau: `task.spawn(function()
	task.wait(2)
	print("Delay finished!")
end)`,
  see: ["parallel", "await", "async"],
});

add({
  id: "parallel",
  title: "parallel",
  header: "Concurrency",
  summary: "Parallel Luau block. Emits `task.desynchronize` / `task.synchronize`.",
  syntax: `parallel {
    ComputeComplexPhysics();
};`,
  params: [],
  returns: "None.",
  emit: "task.desynchronize() … task.synchronize()",
  description: `Actors and thread safety are Roblox engine rules. Do not touch Instances unsafely inside the block. See Roblox Parallel Luau docs for what is legal.`,
  exampleClpp: `parallel {
    ComputeComplexPhysics();
};`,
  exampleLuau: `task.desynchronize()
ComputeComplexPhysics()
task.synchronize()`,
  see: ["spawn", "async"],
});

add({
  id: "destructure",
  title: "auto [a, b] =",
  sidebar: "destructure",
  header: "Concurrency / multiple returns",
  summary: "Unpack multiple return values. The usual pairing with `pcall`.",
  syntax: "auto [a, b] = expr;",
  params: [],
  returns: "Each name is a local.",
  emit: "local a, b = expr",
  description: `Not structured bindings for structs. Not \`auto [x, y]\` on a table field unpack unless \`expr\` returns multiple values.`,
  exampleClpp: `auto [success, result] = pcall(func () {
    return DataStore.GetAsync("PlayerData");
});`,
  exampleLuau: `local success, result = pcall(function()
	return DataStore:GetAsync("PlayerData")
end)`,
  see: ["pcall", "auto"],
});

add({
  id: "struct",
  title: "struct / class",
  sidebar: "struct",
  header: "OOP",
  summary:
    "Declare the type in a `.clh`. Implement `Class::Method` in the sibling `.clp` / `.clpp`.",
  syntax: `struct LeaderstatsServer {
    Janitor janitor;
    void OnPlayer(Player player);
};`,
  params: [],
  returns: "A type (and, for field-only headers, a constructor function).",
  emit: "export type + function Class:Method / const function Name() for field-only",
  description: `\`class\` is a synonym of \`struct\`. [\`public:\` / \`private:\` / \`protected:\`](access-labels) are ignored.

Field-only structs in a header emit \`const function Name()\` with defaults (DataStore templates). Nested structs with defaults become nested tables.

Stems must match: \`LeaderstatsServer.clh\` beside \`LeaderstatsServer.server.clpp\`.

Instances are not RAII. Leaving a block does **not** \`Destroy\` — use Janitor.`,
  exampleClpp: `struct LeaderstatsServer {
    Janitor janitor;
};`,
  exampleLuau: `-- export type LeaderstatsServer = { janitor: Janitor, ... }`,
  see: ["class-method", "this", "init", "access-labels"],
});

add({
  id: "class-method",
  title: "Class::Method",
  sidebar: "Class::Method",
  header: "OOP",
  summary: "Method implementation. Emits `function Class:Method(...)`.",
  syntax: `void Class::Method(T arg) {
    @field = arg;
}`,
  params: [],
  returns: "Per signature.",
  emit: "function Class:Method(arg: T)",
  description: `[\`this\`](this) / [\`@this\`](this) is \`self\`. Bare fields become \`self.field\`. \`@janitor\` is \`self.janitor\`. Untagged files with only \`Class::\` \`return\` the table (ModuleScript).

Construct **one** service in [\`init()\`](init) and use it from lambdas.`,
  exampleClpp: `void LeaderstatsServer::OnPlayer(Player player) {
    post(player.Name);
}`,
  exampleLuau: `function LeaderstatsServer:OnPlayer(player: Player)
	print(player.Name)
end`,
  see: ["struct", "this", "init", "operator-method"],
});

add({
  id: "access-labels",
  title: "public / private / protected",
  sidebar: "public:",
  header: "OOP",
  summary: "Parsed and ignored. Luau has no access specifiers.",
  syntax: `struct S {
public:
    int x;
private:
    int y;
};`,
  params: [],
  returns: "None.",
  emit: "(omitted)",
  description: `They exist so C++-looking headers parse. Encapsulation is by ModuleScript surface, not the compiler.`,
  exampleClpp: `struct S {
public:
    int x;
};`,
  exampleLuau: `-- fields still emit on the type`,
  see: ["struct"],
});

/* ───────── signals ───────── */
add({
  id: "Fire",
  title: "Fire",
  header: "Signals",
  summary: "Send a payload to every current listener of a `signal`.",
  syntax: "name.Fire(...);",
  params: [{ name: "...", type: "T...", desc: "Must match `signal<T...>`." }],
  returns: "`void`.",
  emit: "name:Fire(...)",
  description: `This is **send**. Listening is [\`Connect\`](Connect) / [\`Once\`](Once) / [\`~>\`](operator-janitor).`,
  exampleClpp: `signal<Player, int> OnCoinsUpdated;
OnCoinsUpdated.Fire(player, 500);`,
  exampleLuau: `local OnCoinsUpdated = __signal()
OnCoinsUpdated:Fire(player, 500)`,
  see: ["signal-type", "Connect", "Once", "Wait"],
});

add({
  id: "Connect",
  title: "Connect",
  header: "Signals",
  summary: "Subscribe until Disconnect. Prefer `~>Connect` so Janitor owns the connection.",
  syntax: `name::Connect(fn);
name~>Connect(fn);`,
  params: [{ name: "fn", type: "func", desc: "Listener. Arguments match the signal payload." }],
  returns: "A connection with `:Disconnect()`.",
  emit: "name:Connect(fn)  — or janitor:Add(..., \"Disconnect\")",
  description: `Works on \`signal<T>\`, RBXScriptSignals (\`PlayerAdded\`), and anything with \`:Connect\`.`,
  exampleClpp: `players.PlayerAdded::Connect(func (Player player) {
    post(player.Name);
});`,
  exampleLuau: `players.PlayerAdded:Connect(function(player: Player)
	print(player.Name)
end)`,
  see: ["Once", "operator-janitor", "Fire", "lambda"],
});

add({
  id: "Once",
  title: "Once",
  header: "Signals",
  summary: "Subscribe for a **single** emission, then disconnect. `~>Once` is janitor-managed.",
  syntax: `name::Once(fn);
name~>Once(fn);`,
  params: [{ name: "fn", type: "func", desc: "Listener." }],
  returns: "A connection.",
  emit: "name:Once(fn)",
  description: `Use for “first player only”, one-shot setup, or handshake events.`,
  exampleClpp: `players.PlayerAdded~>Once(func (Player player) {
    post("First player only");
});`,
  exampleLuau: `janitor:Add(players.PlayerAdded:Once(function(player: Player)
	print("First player only")
end), "Disconnect")`,
  see: ["Connect", "operator-janitor", "Fire"],
});

add({
  id: "Wait",
  title: "Wait",
  header: "Signals",
  summary: "Yield until the next `Fire` (or next RBXScriptSignal fire).",
  syntax: "auto payload = name.Wait();",
  params: [],
  returns: "The next payload (possibly multiple values).",
  emit: "name:Wait()",
  description: `Blocks the current thread. Prefer [\`Connect\`](Connect) for ongoing work. Combine with [\`await\`](await) only if \`Wait\` is Promise-like — engine signals yield directly.`,
  exampleClpp: `OnCoinsUpdated.Wait();`,
  exampleLuau: `OnCoinsUpdated:Wait()`,
  see: ["Fire", "Connect", "async"],
});

add({
  id: "GetPropertyChangedSignal",
  title: "GetPropertyChangedSignal",
  header: "Signals · Instance",
  summary: "Engine signal for one Instance property. Listen with `~>` or `::Connect`.",
  syntax: `obj.GetPropertyChangedSignal("Name")~>Connect(fn);`,
  params: [{ name: "name", type: "string", desc: "Property name, e.g. `\"Transparency\"`." }],
  returns: "An RBXScriptSignal.",
  emit: "obj:GetPropertyChangedSignal(\"Name\")",
  description: `The callback receives no property value on some engine signals — read \`obj.Property\` inside the listener.`,
  exampleClpp: `part.GetPropertyChangedSignal("Transparency")~>Connect(func () {
    post(part.Transparency);
});`,
  exampleLuau: `part:GetPropertyChangedSignal("Transparency"):Connect(function()
	print(part.Transparency)
end)`,
  see: ["Connect", "OnChange", "operator-property"],
});

add({
  id: "OnChange",
  title: "OnChange",
  header: "Observables",
  summary: "Listen to an `observable`'s `Changed`. Argument is the new `.Value`.",
  syntax: "name.OnChange(fn);",
  params: [{ name: "fn", type: "func", desc: "`func (T newValue) { }`." }],
  returns: "A connection.",
  emit: "name.Changed:Connect(fn)",
  description: `Only for [\`observable T\`](observable). For arbitrary Instance properties use [\`GetPropertyChangedSignal\`](GetPropertyChangedSignal).`,
  exampleClpp: `observable int coins = 100;
coins.OnChange(func (int newValue) {
    post("now " .: newValue);
});`,
  exampleLuau: `coins.Changed:Connect(function(newValue: number)
	print("now " .. newValue)
end)`,
  see: ["observable", "GetPropertyChangedSignal"],
});

/* ───────── attributes / preprocessor / headers ───────── */
add({
  id: "attr-server",
  title: "[[server]]",
  sidebar: "[[server]]",
  header: "Attribute",
  summary: "Function exists on the server only.",
  syntax: `[[server]]
void SaveData(Player player) { }`,
  params: [],
  returns: "Per function.",
  emit: "omitted on client files; RunService:IsServer() wrapper in modules",
  description: `On a \`.client.clpp\`, the function is omitted. On a \`.server.clpp\`, it emits normally. In a module, emit wraps \`RunService:IsServer()\`.`,
  exampleClpp: `[[server]]
void SaveData(Player player) {}`,
  exampleLuau: `-- emitted only when the file is a server Script / IsServer()`,
  see: ["attr-client", "../files"],
});

add({
  id: "attr-client",
  title: "[[client]]",
  sidebar: "[[client]]",
  header: "Attribute",
  summary: "Function exists on the client only.",
  syntax: `[[client]]
void UpdateUI() { }`,
  params: [],
  returns: "Per function.",
  emit: "omitted on server files; RunService:IsClient() wrapper in modules",
  description: `Mirror of [\`[[server]]\`](attr-server).`,
  exampleClpp: `[[client]]
void UpdateUI() {}`,
  exampleLuau: `-- emitted only when the file is a LocalScript / IsClient()`,
  see: ["attr-server", "../files"],
});

add({
  id: "include",
  title: "#include",
  header: "Preprocessor",
  summary:
    "Angle-bracket includes are IntelliSense (and `require` for libs). Quoted sibling stem is inlined.",
  syntax: `#include <clpp/roblox.clh>
#include "LeaderstatsServer.clh"`,
  params: [{ name: "path", type: "string", desc: "Header path." }],
  returns: "None.",
  emit: "require(…) for libs / non-stem quotes; stem quote is inlined",
  description: `**Quoted, same stem** as the \`.clpp\`: the header is **inlined**.

**Quoted, any other name**: \`require\`.

**Angle** \`<clpp/libs/janitor.clh>\`: IntelliSense **and** \`require\` Janitor. \`<clpp/roblox.clh>\` is IntelliSense only (engine globals).

See [headers](header-roblox).`,
  exampleClpp: `#include <clpp/roblox.clh>
#include <clpp/libs/janitor.clh>`,
  exampleLuau: `local Janitor = require(...)`,
  see: [
    "header-roblox",
    "header-janitor",
    "pragma-strict",
  ],
});

add({
  id: "pragma-strict",
  title: "#pragma strict",
  sidebar: "pragma strict",
  header: "Preprocessor",
  summary: "Emit `--!strict` at the top of the Luau file.",
  syntax: "#pragma strict",
  params: [],
  returns: "None.",
  emit: "--!strict",
  description: `Default is **not** strict unless config \`"strict": true\`. [\`#pragma nstrict\`](pragma-nstrict) never emits the comment.`,
  exampleClpp: `#pragma strict
void init() {}`,
  exampleLuau: `--!strict`,
  see: ["pragma-nstrict", "pragma-once", "include"],
});

add({
  id: "pragma-nstrict",
  title: "#pragma nstrict",
  sidebar: "pragma nstrict",
  header: "Preprocessor",
  summary: "Never emit `--!strict`, even if the project config asks for it.",
  syntax: "#pragma nstrict",
  params: [],
  returns: "None.",
  emit: "(no --!strict)",
  description: `Use on files that still need Looser Luau.`,
  exampleClpp: `#pragma nstrict`,
  exampleLuau: `-- (no directive)`,
  see: ["pragma-strict"],
});

add({
  id: "pragma-once",
  title: "#pragma once",
  sidebar: "pragma once",
  header: "Preprocessor",
  summary: "Parsed and ignored. Headers are not multiply included the C++ way.",
  syntax: "#pragma once",
  params: [],
  returns: "None.",
  emit: "(ignored)",
  description: `Safe to write at the top of \`.clh\` files for clangd / habit. The compiler does not implement include guards.`,
  exampleClpp: `#pragma once
struct Data {};`,
  exampleLuau: `-- struct emit only`,
  see: ["include", "pragma-strict"],
});

add({
  id: "header-roblox",
  title: "<clpp/roblox.clh>",
  sidebar: "roblox.clh",
  header: "Standard header",
  summary: "Engine globals for IntelliSense. Does not emit `require`.",
  syntax: "#include <clpp/roblox.clh>",
  params: [],
  returns: "None.",
  emit: "(no require)",
  description: `Gives the editor \`game\`, \`workspace\`, services, and the usual Roblox globals. Runtime wiring of generated Instance classes is [Cluaupp](https://github.com/KartzRbx/Cluaupp).`,
  exampleClpp: `#include <clpp/roblox.clh>
void init() {
    Players players = GetService<Players>();
}`,
  exampleLuau: `local players: Players = game:GetService("Players")`,
  see: ["GetService", "include", "header-instances"],
});

add({
  id: "header-instances",
  title: "<clpp/generated/instances.clh>",
  sidebar: "instances.clh",
  header: "Standard header",
  summary: "Generated Instance class names for IntelliSense (Cluaupp).",
  syntax: "#include <clpp/generated/instances.clh>",
  params: [],
  returns: "None.",
  emit: "(IntelliSense)",
  description: `Produced by Cluaupp from the Roblox API dump. CL++ the language does not ship a live dump.`,
  exampleClpp: `#include <clpp/generated/instances.clh>`,
  exampleLuau: `-- types only`,
  see: ["header-roblox", "new"],
});

add({
  id: "header-datatypes",
  title: "<clpp/datatypes.clh>",
  sidebar: "datatypes.clh",
  header: "Standard header",
  summary: "Vector3, CFrame, UDim2, Color3, and `string_concat`.",
  syntax: "#include <clpp/datatypes.clh>",
  params: [],
  returns: "None.",
  emit: "datatype constructors stay global",
  description: `Datatypes are **values**: \`Vector3(8, 1, 8)\`, not \`new Vector3\`. Methods/static names use [\`::\`](operator-method) which emits \`.\` for datatypes.`,
  exampleClpp: `part.Size = Vector3(8, 1, 8);
part.CFrame = CFrame.lookAt(from, look);`,
  exampleLuau: `part.Size = Vector3.new(8, 1, 8)
part.CFrame = CFrame.lookAt(from, look)`,
  see: ["new", "string_concat", "operator-method"],
});

add({
  id: "header-janitor",
  title: "<clpp/libs/janitor.clh>",
  sidebar: "janitor.clh",
  header: "Standard header",
  summary: "IntelliSense **and** `require` of Janitor. Needed for [`~>`](operator-janitor).",
  syntax: "#include <clpp/libs/janitor.clh>",
  params: [],
  returns: "None.",
  emit: "require(Janitor)",
  description: `\`new Janitor()\` emits \`Janitor.new()\`. \`~>\` looks up a janitor in scope.`,
  exampleClpp: `#include <clpp/libs/janitor.clh>
auto janitor = new Janitor();`,
  exampleLuau: `local Janitor = require(...)
local janitor = Janitor.new()`,
  see: ["operator-janitor", "new", "include"],
});

add({
  id: "header-dataservice",
  title: "<clpp/libs/dataservice.clh>",
  sidebar: "dataservice.clh",
  header: "Standard header",
  summary: "DataService table (`.Server` / `.Client`) plus `require`.",
  syntax: "#include <clpp/libs/dataservice.clh>",
  params: [],
  returns: "None.",
  emit: "require(DataService)",
  description: `Access the singleton with [\`.\`](operator-property): \`DataService.Server.WaitFor(player)\`.`,
  exampleClpp: `DataService.Server.Init(DataServiceOptions {
    .Template = playerData,
    .StoreName = "PlayerData",
    .UseMock = true
});`,
  exampleLuau: `DataService.Server:Init({
	Template = playerData,
	StoreName = "PlayerData",
	UseMock = true
})`,
  see: ["operator-table", "operator-designated", "await"],
});

add({
  id: "language-id",
  title: "Language id clpp",
  sidebar: "clpp fence",
  header: "Tooling",
  summary:
    "How CL++ is registered as a language — editors, this documentation site, and GitHub.",
  syntax: 'post("hello");',
  params: [],
  returns: "N/A",
  emit: "N/A",
  description: `CL++ is **not** C++. Fences must use the language id \`clpp\` (aliases \`clp\`, \`clh\`).

**Editors.** \`clpp install\` copies the VS Code / Cursor pack. \`package.json\` contributes language id \`clpp\` for \`*.clpp\` / \`*.clp\` / \`*.clh\`, with grammar \`source.clpp\`. That is a TextMate registry, not Prism.

**This site (Moonwave / Docusaurus).** Prism does not ship \`clpp\`. The docs build injects \`src/theme/prism-include-languages.js\`, loads Prism's C++ grammar, and aliases \`clpp\`. The API tab uses Refractor via \`@mapbox/rehype-prism\`; the same build aliases \`cpp → clpp\`. Without that registration, SSG throws \`Unknown language: clpp is not registered\`.

**GitHub.com.** [Linguist](https://github.com/github-linguist/linguist) highlights files from extensions. \`.gitattributes\` maps \`*.clpp\` to C++ so repository blobs have syntax color. Markdown fences on github.com stay plain until Linguist accepts a custom language. That is a separate registry from this site.

**How to add a new highlighter.** (1) Editors: \`editors/vscode/syntaxes/clpp.tmLanguage.json\`. (2) Docs site: \`.moonwave/src/theme/prism-include-languages.js\` plus \`scripts/moonwave-docs.cjs\`. (3) GitHub: a Linguist PR, not something this repo can finish alone.`,
  exampleClpp: `post("hello");`,
  exampleLuau: `print("hello")`,
  notes:
    "Do not use the fence language `cpp` for CL++ samples on this site. The fence id is part of the language identity.",
  see: [
    ["Install", "../install"],
    "post",
  ],
});

const index = `---
title: Language reference
---

# Language reference

This is the CL++ library and syntax reference — the same job [cplusplus.com/reference](https://cplusplus.com/reference/) does for the C++ standard library. Every utility has its own page: syntax, parameters, return value, Luau emit, example, and see-also.

The generated **API** tab ([Builtins](/CLPP/api/Builtins), [Operators](/CLPP/api/Operators), …) is the same surface grouped as Moonwave classes. Start here if you want a header-style index.

<div class="clpp-ref-index">

<div>
<h3>I/O</h3>
<ul>
<li><a href="/CLPP/docs/reference/post">post</a></li>
<li><a href="/CLPP/docs/reference/warn">warn</a></li>
<li><a href="/CLPP/docs/reference/report">report</a></li>
<li><a href="/CLPP/docs/reference/cout">cout / endl</a></li>
</ul>
</div>

<div>
<h3>Builtins</h3>
<ul>
<li><a href="/CLPP/docs/reference/GetService">GetService</a></li>
<li><a href="/CLPP/docs/reference/pcall">pcall</a></li>
<li><a href="/CLPP/docs/reference/new">new</a></li>
<li><a href="/CLPP/docs/reference/static_cast">static_cast</a></li>
<li><a href="/CLPP/docs/reference/string_concat">string_concat</a></li>
<li><a href="/CLPP/docs/reference/null">null</a></li>
<li><a href="/CLPP/docs/reference/init">void init()</a></li>
<li><a href="/CLPP/docs/reference/this">this</a></li>
</ul>
</div>

<div>
<h3>Types</h3>
<ul>
<li><a href="/CLPP/docs/reference/int">int</a></li>
<li><a href="/CLPP/docs/reference/float">float</a></li>
<li><a href="/CLPP/docs/reference/double">double</a></li>
<li><a href="/CLPP/docs/reference/bool">bool</a></li>
<li><a href="/CLPP/docs/reference/string">string</a></li>
<li><a href="/CLPP/docs/reference/void">void</a></li>
<li><a href="/CLPP/docs/reference/func">func</a></li>
<li><a href="/CLPP/docs/reference/auto">auto</a></li>
<li><a href="/CLPP/docs/reference/optional">optional&lt;T&gt;</a></li>
<li><a href="/CLPP/docs/reference/array">array&lt;T&gt;</a></li>
<li><a href="/CLPP/docs/reference/vector">vector&lt;T&gt;</a></li>
<li><a href="/CLPP/docs/reference/dictionary">dictionary&lt;K,V&gt;</a></li>
<li><a href="/CLPP/docs/reference/instance-pointer">Instance types</a></li>
<li><a href="/CLPP/docs/reference/const">const</a></li>
<li><a href="/CLPP/docs/reference/signal-type">signal&lt;T...&gt;</a></li>
<li><a href="/CLPP/docs/reference/observable">observable T</a></li>
</ul>
</div>

<div>
<h3>Operators</h3>
<ul>
<li><a href="/CLPP/docs/reference/operator-method">:: static / manual Connect</a></li>
<li><a href="/CLPP/docs/reference/operator-table">: type / protected call</a></li>
<li><a href="/CLPP/docs/reference/operator-property">. property / instance method</a></li>
<li><a href="/CLPP/docs/reference/operator-concat">.: concat</a></li>
<li><a href="/CLPP/docs/reference/operator-janitor">~&gt; janitor</a></li>
<li><a href="/CLPP/docs/reference/operator-arithmetic">+ − * /</a></li>
<li><a href="/CLPP/docs/reference/operator-comparison">== != &lt; &gt;</a></li>
<li><a href="/CLPP/docs/reference/operator-logic">&amp;&amp; || !</a></li>
<li><a href="/CLPP/docs/reference/operator-assignment">= += …</a></li>
<li><a href="/CLPP/docs/reference/operator-increment">++ −−</a></li>
<li><a href="/CLPP/docs/reference/operator-designated">.Field =</a></li>
</ul>
</div>

<div>
<h3>Control flow</h3>
<ul>
<li><a href="/CLPP/docs/reference/if">if / else if / else</a></li>
<li><a href="/CLPP/docs/reference/while">while</a></li>
<li><a href="/CLPP/docs/reference/for">for (C-style)</a></li>
<li><a href="/CLPP/docs/reference/range-for">for (T x in list)</a></li>
<li><a href="/CLPP/docs/reference/switch">switch</a></li>
<li><a href="/CLPP/docs/reference/guard">guard</a></li>
<li><a href="/CLPP/docs/reference/match">match</a></li>
<li><a href="/CLPP/docs/reference/break">break</a></li>
<li><a href="/CLPP/docs/reference/return">return</a></li>
</ul>
</div>

<div>
<h3>Functions &amp; OOP</h3>
<ul>
<li><a href="/CLPP/docs/reference/function">function</a></li>
<li><a href="/CLPP/docs/reference/lambda">func (...)</a></li>
<li><a href="/CLPP/docs/reference/struct">struct / class</a></li>
<li><a href="/CLPP/docs/reference/class-method">Class::Method</a></li>
<li><a href="/CLPP/docs/reference/access-labels">public / private</a></li>
</ul>
</div>

<div>
<h3>Signals</h3>
<ul>
<li><a href="/CLPP/docs/reference/Fire">Fire</a></li>
<li><a href="/CLPP/docs/reference/Connect">Connect</a></li>
<li><a href="/CLPP/docs/reference/Once">Once</a></li>
<li><a href="/CLPP/docs/reference/Wait">Wait</a></li>
<li><a href="/CLPP/docs/reference/GetPropertyChangedSignal">GetPropertyChangedSignal</a></li>
<li><a href="/CLPP/docs/reference/OnChange">OnChange</a></li>
</ul>
</div>

<div>
<h3>Concurrency</h3>
<ul>
<li><a href="/CLPP/docs/reference/async">async</a></li>
<li><a href="/CLPP/docs/reference/await">await</a></li>
<li><a href="/CLPP/docs/reference/spawn">spawn</a></li>
<li><a href="/CLPP/docs/reference/parallel">parallel</a></li>
<li><a href="/CLPP/docs/reference/destructure">auto [a, b] =</a></li>
</ul>
</div>

<div>
<h3>Attributes &amp; preprocessor</h3>
<ul>
<li><a href="/CLPP/docs/reference/attr-server">[[server]]</a></li>
<li><a href="/CLPP/docs/reference/attr-client">[[client]]</a></li>
<li><a href="/CLPP/docs/reference/include">#include</a></li>
<li><a href="/CLPP/docs/reference/pragma-strict">#pragma strict</a></li>
<li><a href="/CLPP/docs/reference/pragma-nstrict">#pragma nstrict</a></li>
<li><a href="/CLPP/docs/reference/pragma-once">#pragma once</a></li>
</ul>
</div>

<div>
<h3>Headers</h3>
<ul>
<li><a href="/CLPP/docs/reference/header-roblox">&lt;clpp/roblox.clh&gt;</a></li>
<li><a href="/CLPP/docs/reference/header-instances">&lt;clpp/generated/instances.clh&gt;</a></li>
<li><a href="/CLPP/docs/reference/header-datatypes">&lt;clpp/datatypes.clh&gt;</a></li>
<li><a href="/CLPP/docs/reference/header-janitor">&lt;clpp/libs/janitor.clh&gt;</a></li>
<li><a href="/CLPP/docs/reference/header-dataservice">&lt;clpp/libs/dataservice.clh&gt;</a></li>
</ul>
</div>

<div>
<h3>Tooling</h3>
<ul>
<li><a href="/CLPP/docs/reference/language-id"><code>clpp</code> language id</a></li>
</ul>
</div>

</div>

## Not in the language

See [What CL++ does not do](unsupported): \`->\`, \`continue\`, ternary, \`do/while\`, \`try/catch\`, \`goto\`, pointer arithmetic, C++ captures, generic templates, \`std::\`.
`;

// Index layout (two-column lists + header quick access) is maintained in docs/reference.md.
for (const p of pages) {
  fs.writeFileSync(path.join(OUT, `${p.id}.md`), page(p));
}
console.log(`wrote ${pages.length} reference pages (index layout is docs/reference.md)`);
