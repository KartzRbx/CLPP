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
    Class: vscode.CompletionItemKind.Class,
    Operator: vscode.CompletionItemKind.Operator,
  };
  return map[name] || vscode.CompletionItemKind.Text;
}

function activate(context) {
  const data = loadCompletions();
  const selector = [
    { language: "clpp", scheme: "file" },
    { language: "clpp", scheme: "untitled" },
  ];

  const completion = vscode.languages.registerCompletionItemProvider(selector, {
    provideCompletionItems() {
      const items = [];
      for (const word of data.keywords || []) {
        const item = new vscode.CompletionItem(word, vscode.CompletionItemKind.Keyword);
        items.push(item);
      }
      for (const fn of data.builtins || []) {
        const item = new vscode.CompletionItem(fn.label, kindOf(fn.kind));
        item.detail = fn.detail;
        items.push(item);
      }
      for (const ty of data.types || []) {
        const item = new vscode.CompletionItem(ty, vscode.CompletionItemKind.Class);
        items.push(item);
      }
      for (const op of data.operators || []) {
        const item = new vscode.CompletionItem(op.label, vscode.CompletionItemKind.Operator);
        item.detail = op.detail;
        items.push(item);
      }
      return items;
    },
  });

  const hover = vscode.languages.registerHoverProvider(selector, {
    provideHover(document, position) {
      const word = document.getText(document.getWordRangeAtPosition(position));
      const ops = {
        post: "Standard output → Luau `print`",
        warn: "Warning → Luau `warn`",
        report: "Exception → Luau `error`",
        observable: "Reactive ValueBase (`IntValue`, `NumberValue`, …)",
        signal: "Typed signal (`__signal()` / BindableEvent)",
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
