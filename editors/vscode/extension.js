const vscode = require("vscode");
const path = require("path");
const fs = require("fs");

function loadCompletions() {
  const file = path.join(__dirname, "data", "completions.json");
  try {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  } catch {
    return { keywords: [], builtins: [], types: [], operators: [] };
  }
}

function kindOf(name) {
  const map = {
    Keyword: vscode.CompletionItemKind.Keyword,
    Function: vscode.CompletionItemKind.Function,
    Method: vscode.CompletionItemKind.Method,
    Property: vscode.CompletionItemKind.Property,
    Class: vscode.CompletionItemKind.Class,
    Operator: vscode.CompletionItemKind.Operator,
    Variable: vscode.CompletionItemKind.Variable,
  };
  return map[name] || vscode.CompletionItemKind.Text;
}

function item(label, kind, detail) {
  const completion = new vscode.CompletionItem(label, kindOf(kind));
  completion.detail = detail;
  return completion;
}

function localsFrom(document) {
  const names = new Set();
  const text = document.getText();
  const decl =
    /\b(?:observable\s+)?(?:signal(?:<[^;\n]+>)?|int|float|double|bool|string|auto|func|array(?:<[^;\n]+>)?|dictionary(?:<[^;\n]+>)?|[A-Z][A-Za-z0-9_]*\*?)\s+(\w+)/g;
  let match;
  while ((match = decl.exec(text))) {
    names.add(match[1]);
  }
  return names;
}

function prefixOf(line) {
  if (/~>\s*[A-Za-z_]*$/.test(line)) {
    return "cleanup";
  }
  if (/::[A-Za-z_]*$/.test(line)) {
    return "method";
  }
  if (/\.[A-Za-z_]*$/.test(line)) {
    return "property";
  }
  if (/(^|[^:]):[A-Za-z_]*$/.test(line)) {
    return "table";
  }
  return "global";
}

function activate(context) {
  const data = loadCompletions();
  const selector = [
    { language: "clpp", scheme: "file" },
    { language: "clpp", scheme: "untitled" },
  ];

  const completion = vscode.languages.registerCompletionItemProvider(
    selector,
    {
      provideCompletionItems(document, position) {
        const line = document.lineAt(position).text.slice(0, position.character);
        const mode = prefixOf(line);
        const items = [];

        if (mode === "cleanup") {
          for (const fn of data.cleanup || []) {
            items.push(item(fn.label, "Method", fn.detail));
          }
          return items;
        }

        if (mode === "method") {
          for (const fn of data.methods || []) {
            items.push(item(fn.label, "Method", fn.detail));
          }
          return items;
        }

        if (mode === "property") {
          for (const prop of data.properties || []) {
            items.push(item(prop.label, "Property", prop.detail));
          }
          for (const fn of data.methods || []) {
            if (fn.label === "OnChange" || fn.label === "Fire") {
              items.push(item(fn.label, "Method", fn.detail));
            }
          }
          return items;
        }

        if (mode === "table") {
          items.push(item("Server", "Property", "DataService:Server"));
          items.push(item("Client", "Property", "DataService:Client"));
          items.push(item("Paths", "Property", "table key"));
          return items;
        }

        for (const word of data.keywords || []) {
          items.push(item(word, "Keyword", "CL++ keyword"));
        }
        for (const fn of data.builtins || []) {
          items.push(item(fn.label, fn.kind || "Function", fn.detail));
        }
        for (const ty of data.types || []) {
          items.push(item(ty, "Class", "CL++ / Roblox type"));
        }
        for (const op of data.operators || []) {
          items.push(item(op.label, "Operator", op.detail));
        }
        for (const name of localsFrom(document)) {
          items.push(item(name, "Variable", "local"));
        }
        return items;
      },
    },
    ".",
    ":",
    ">"
  );

  const hover = vscode.languages.registerHoverProvider(selector, {
    provideHover(document, position) {
      const word = document.getText(document.getWordRangeAtPosition(position));
      const ops = {
        post: "Standard output → Luau `print`",
        warn: "Warning → Luau `warn`",
        report: "Exception → Luau `error`",
        observable:
          "Reactive ValueBase. Assign to fire a change (`coins = 50`); listen with `.OnChange`.",
        signal:
          "`signal<T...>` — `::Fire(...)` emits, `~>Connect` / `~>Once` subscribe (Once runs once).",
        Fire: "`signal::Fire(...)` → BindableEvent:Fire — send the signal.",
        Connect: "Subscribe until Disconnect. Prefer `~>Connect` for Janitor cleanup.",
        Once: "Subscribe for a single emission, then disconnect. `~>Once` is janitor-managed.",
        Wait: "Yield until the next emission (`RBXScriptSignal:Wait`).",
        OnChange: "observable listener → `Changed:Connect`.",
        GetPropertyChangedSignal:
          '`instance::GetPropertyChangedSignal("Name")` — property change signal.',
        guard: "`if not (cond) then … end`",
        await: "Wait for a Promise (`:expect()`) or an already-yielded value",
      };
      if (ops[word]) {
        return new vscode.Hover(ops[word]);
      }
      return null;
    },
  });

  context.subscriptions.push(completion, hover);
}

function deactivate() {}

module.exports = { activate, deactivate };
