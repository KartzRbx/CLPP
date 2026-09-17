const vscode = require("vscode");
const path = require("path");
const fs = require("fs");
const cp = require("child_process");
const { loadEngine, lintDocument } = require("./intellisense");

function loadCompletions() {
  const file = path.join(__dirname, "data", "completions.json");
  try {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  } catch {
    return { keywords: [], builtins: [], types: [], operators: [], catalog: {}, globals: {} };
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
    Event: vscode.CompletionItemKind.Event,
    Field: vscode.CompletionItemKind.Field,
    tableKeys: vscode.CompletionItemKind.Field,
    properties: vscode.CompletionItemKind.Property,
    methods: vscode.CompletionItemKind.Method,
    events: vscode.CompletionItemKind.Event,
  };
  return map[name] || vscode.CompletionItemKind.Text;
}

function item(label, kind, detail, sortPrefix) {
  const completion = new vscode.CompletionItem(label, kindOf(kind));
  completion.detail = detail;
  if (sortPrefix) {
    completion.sortText = `${sortPrefix}${label}`;
  }
  return completion;
}

function memberItem(member) {
  const kind =
    member.kind === "events"
      ? "Event"
      : member.kind === "methods"
        ? "Method"
        : member.kind === "tableKeys"
          ? "Field"
          : "Property";
  const sort =
    member.kind === "tableKeys" || member.kind === "properties"
      ? "0_"
      : member.kind === "events"
        ? "1_"
        : "2_";
  return item(member.label, kind, member.detail, sort);
}

const HOVER_WORDS = {
  post: "Write a line to output.",
  warn: "Write a warning.",
  report: "Stop the script with an error.",
  observable:
    "Reactive value. Assign to update (`coins = 50`); listen with `.OnChange`.",
  signal:
    "`signal<T...>` — `::Fire(...)` sends, `~>Connect` / `~>Once` listen (Once runs once).",
  Fire: "`signal::Fire(...)` — send the signal.",
  Connect: "Subscribe until Disconnect. Prefer `~>Connect` for automatic cleanup.",
  Once: "Subscribe for a single emission, then disconnect. `~>Once` is cleaned up for you.",
  Wait: "Pause until the next emission.",
  OnChange: "Run a function whenever an observable changes.",
  GetPropertyChangedSignal:
    '`instance::GetPropertyChangedSignal("Name")` — signal for one property.',
  guard: "Continue only when the condition is true; otherwise run the else block.",
  await: "Wait until an async value is ready.",
};

function activate(context) {
  const data = loadCompletions();
  const engine = loadEngine(data);
  const selector = [
    { language: "clpp", scheme: "file" },
    { language: "clpp", scheme: "untitled" },
  ];

  const completion = vscode.languages.registerCompletionItemProvider(
    selector,
    {
      provideCompletionItems(document, position) {
        const line = document.lineAt(position).text.slice(0, position.character);
        const symbols = engine.indexDocument(document.getText());
        const items = [];

        if (/GetService\s*<\s*[A-Za-z_]*$/.test(line)) {
          for (const ty of data.services || data.types || []) {
            items.push(item(ty, "Class", `GetService<${ty}>()`, "0_"));
          }
          return items;
        }

        const resolved = engine.resolve(line, symbols);
        if (resolved.mode !== "global" && resolved.mode !== "concat") {
          for (const member of resolved.members) {
            items.push(memberItem(member));
          }
          return items;
        }

        for (const word of data.keywords || []) {
          items.push(item(word, "Keyword", "CL++ keyword", "2_"));
        }
        for (const fn of data.builtins || []) {
          items.push(item(fn.label, fn.kind || "Function", fn.detail, "1_"));
        }
        for (const ty of data.types || []) {
          items.push(item(ty, "Class", "CL++ / Roblox type", "3_"));
        }
        for (const op of data.operators || []) {
          items.push(item(op.label, "Operator", op.detail, "4_"));
        }
        for (const [name, info] of symbols.vars) {
          items.push(item(name, "Variable", info.detail, "0_"));
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
      const range = document.getWordRangeAtPosition(position);
      if (!range) {
        return null;
      }
      const word = document.getText(range);
      if (HOVER_WORDS[word]) {
        return new vscode.Hover(HOVER_WORDS[word]);
      }
      const symbols = engine.indexDocument(document.getText());
      const detail = engine.hoverFor(word, symbols);
      if (detail) {
        return new vscode.Hover(detail);
      }
      return null;
    },
  });

  const collection = vscode.languages.createDiagnosticCollection("clpp");
  let timer;
  function refresh(document) {
    if (!document || document.languageId !== "clpp") {
      return;
    }
    const fromCompiler = clppDiagnostics(document);
    const issues = fromCompiler !== null ? fromCompiler : lintDocument(document.getText());
    collection.set(
      document.uri,
      issues.map((issue) => {
        const line = Math.min(issue.line, Math.max(0, document.lineCount - 1));
        const range = document.lineAt(line).range;
        const start = new vscode.Position(line, issue.column || 0);
        return new vscode.Diagnostic(
          new vscode.Range(start, range.end),
          issue.message,
          vscode.DiagnosticSeverity.Error
        );
      })
    );
  }
  function refreshAll() {
    for (const doc of vscode.workspace.textDocuments) {
      refresh(doc);
    }
  }
  const diagnostics = vscode.workspace.onDidChangeTextDocument((ev) => {
    clearTimeout(timer);
    timer = setTimeout(() => refresh(ev.document), 350);
  });
  context.subscriptions.push(
    collection,
    vscode.workspace.onDidOpenTextDocument(refresh),
    vscode.workspace.onDidCloseTextDocument((doc) => collection.delete(doc.uri))
  );

  context.subscriptions.push(completion, hover, diagnostics);
  refreshAll();
}

function findClpp() {
  const folders = (vscode.workspace.workspaceFolders || []).map((f) => f.uri.fsPath);
  const local = process.env.LOCALAPPDATA || "";
  const names = process.platform === "win32" ? ["clpp.exe"] : ["clpp"];
  const extra = [
    path.join(local, "Programs", "CLPP", "clpp.exe"),
  ];
  for (const root of folders) {
    extra.push(path.join(root, "target", "release", names[0]));
    extra.push(path.join(root, "target", "debug", names[0]));
  }
  for (const candidate of extra) {
    if (candidate && fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return names[0];
}

function clppDiagnostics(document) {
  const exe = findClpp();
  const payload = JSON.stringify({
    source: document.getText(),
    fileName: document.fileName,
  });
  try {
    const result = cp.spawnSync(exe, ["api", "compile"], {
      input: payload,
      encoding: "utf8",
      timeout: 8000,
      windowsHide: true,
    });
    if (result.error) {
      return null;
    }
    const out = (result.stdout || "").trim();
    if (!out) {
      return null;
    }
    const art = JSON.parse(out);
    const items = [];
    for (const d of art.diagnostics || []) {
      items.push({
        line: Math.max(0, (d.line || 1) - 1),
        column: Math.max(0, (d.column || 1) - 1),
        message: d.message,
      });
    }
    if (!items.length && art.ok === false && art.error) {
      const match = String(art.error).match(/[:\[](\d+)[:.](\d+)/);
      items.push({
        line: match ? Math.max(0, Number(match[1]) - 1) : 0,
        column: match ? Math.max(0, Number(match[2]) - 1) : 0,
        message: String(art.error)
          .split("\n")
          .map((l) => l.trim())
          .find((l) => l && !l.startsWith("╭") && !l.startsWith("│") && !l.startsWith("╰") && !l.startsWith("→"))
          || "CL++ error",
      });
    }
    return items;
  } catch {
    return null;
  }
}

function deactivate() {}

module.exports = { activate, deactivate };
