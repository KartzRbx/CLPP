const vscode = require("vscode");
const path = require("path");
const fs = require("fs");
const cp = require("child_process");
const { loadEngine, lintDocument } = require("./intellisense");
const { createLens, applyLensToOpenEditors } = require("./lens");

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
  in: "Range-for: each value comes from the collection after `in`.",
  await: "Wait until an async value is ready.",
  to_string: "Convert a value to text. Emits Luau tostring.",
  to_number: "Parse a number from text. Emits Luau tonumber. Fails → null.",
  to_bool: "Coerce to true/false.",
};

function includeSearchRoots() {
  const roots = [];
  for (const folder of vscode.workspace.workspaceFolders || []) {
    roots.push(folder.uri.fsPath);
    roots.push(path.join(folder.uri.fsPath, "stdlib"));
  }
  roots.push(path.join(__dirname, "..", ".."));
  roots.push(path.join(__dirname, "..", "..", "stdlib"));
  const local = process.env.LOCALAPPDATA || "";
  if (local) {
    roots.push(path.join(local, "Programs", "CLPP"));
    roots.push(path.join(local, "Programs", "CLPP", "stdlib"));
  }
  return roots;
}

function resolveInclude(spec, angled, fromPath) {
  if (!angled && fromPath) {
    const relative = path.normalize(path.join(path.dirname(fromPath), spec));
    if (fs.existsSync(relative)) {
      return relative;
    }
  }
  for (const root of includeSearchRoots()) {
    for (const candidate of [path.join(root, spec), path.join(root, "stdlib", spec)]) {
      if (fs.existsSync(candidate)) {
        return candidate;
      }
    }
  }
  return null;
}

function extraIncludeTexts(document) {
  const texts = [];
  const seen = new Set();
  const visit = (text, fromPath) => {
    const includeRe = /#include\s+(?:"([^"]+)"|<([^>]+)>)/g;
    let match;
    while ((match = includeRe.exec(text))) {
      const spec = match[1] || match[2];
      const resolved = resolveInclude(spec, Boolean(match[2]), fromPath);
      if (!resolved || seen.has(resolved)) {
        continue;
      }
      seen.add(resolved);
      try {
        const body = fs.readFileSync(resolved, "utf8");
        texts.push(body);
        visit(body, resolved);
      } catch {
        // missing include — skip
      }
    }
  };
  const filePath = document.uri.scheme === "file" ? document.uri.fsPath : null;
  visit(document.getText(), filePath);
  return texts;
}

function rangeAt(document, start, end) {
  return new vscode.Range(document.positionAt(start), document.positionAt(end));
}

function eachInclude(document) {
  const text = document.getText();
  const fromPath = document.uri.scheme === "file" ? document.uri.fsPath : null;
  const re = /#include\s+(?:"([^"]+)"|<([^>]+)>)/g;
  const out = [];
  let match;
  while ((match = re.exec(text))) {
    const spec = match[1] || match[2];
    const specIndex = match.index + match[0].lastIndexOf(spec);
    out.push({
      spec,
      angled: Boolean(match[2]),
      resolved: resolveInclude(spec, Boolean(match[2]), fromPath),
      lineRange: rangeAt(document, match.index, match.index + match[0].length),
      specRange: rangeAt(document, specIndex, specIndex + spec.length),
    });
  }
  return out;
}

function structOutline(name, node, range) {
  const symbol = new vscode.DocumentSymbol(name, "struct", vscode.SymbolKind.Struct, range, range);
  for (const field of node.properties || []) {
    symbol.children.push(
      new vscode.DocumentSymbol(field.label, field.type || "", vscode.SymbolKind.Property, range, range)
    );
  }
  for (const method of node.methods || []) {
    symbol.children.push(
      new vscode.DocumentSymbol(method.label, method.returns || "", vscode.SymbolKind.Method, range, range)
    );
  }
  return symbol;
}

function includeOutline(engine, document) {
  const symbols = [];
  for (const inc of eachInclude(document)) {
    const detail = inc.resolved ? path.basename(inc.resolved) : "missing";
    const fileSym = new vscode.DocumentSymbol(
      inc.spec,
      inc.angled ? "include <>" : "include",
      vscode.SymbolKind.File,
      inc.lineRange,
      inc.specRange
    );
    if (inc.resolved) {
      try {
        const body = fs.readFileSync(inc.resolved, "utf8");
        const types = engine.parseStructs(body);
        for (const [name, node] of Object.entries(types)) {
          fileSym.children.push(structOutline(name, node, inc.lineRange));
        }
      } catch {
        // skip unreadable include
      }
    }
    symbols.push(fileSym);
  }
  return symbols;
}

function localOutline(engine, document) {
  const text = document.getText();
  const symbols = [];
  const types = engine.parseStructs(text);
  const structRe = /\bstruct\s+([A-Za-z_]\w*)/g;
  let match;
  const structRanges = new Map();
  while ((match = structRe.exec(text))) {
    structRanges.set(match[1], rangeAt(document, match.index, match.index + match[0].length));
  }
  for (const [name, node] of Object.entries(types)) {
    const range = structRanges.get(name) || document.lineAt(0).range;
    symbols.push(structOutline(name, node, range));
  }
  const methodRe =
    /\b(?:[\w:<\*>\s]+?)\s+([A-Z][A-Za-z0-9_]*)::([A-Za-z_]\w*)\s*\(|\b(void)\s+(init)\s*\(/g;
  while ((match = methodRe.exec(text))) {
    const typeName = match[1] || match[3];
    const methodName = match[2] || match[4];
    const start = match.index + match[0].lastIndexOf(methodName);
    const range = rangeAt(document, start, start + methodName.length);
    const kind = methodName === "init" ? vscode.SymbolKind.Function : vscode.SymbolKind.Method;
    const detail = typeName === "void" ? "script" : typeName;
    symbols.push(new vscode.DocumentSymbol(methodName, detail, kind, range, range));
  }
  return symbols;
}

function symbolsFor(engine, document) {
  return engine.indexDocument(document.getText(), extraIncludeTexts(document));
}

function typeNamesFor(data, symbols) {
  const names = new Set([...(data.types || []), ...Object.keys(data.catalog || {}), ...Object.keys(symbols.types || {})]);
  const skip = new Set(["array", "dictionary", "signal", "observable", "task", "tablelib", "math"]);
  return [...names].filter((name) => /^[A-Z]/.test(name) && !skip.has(name));
}

function codeMask(text) {
  const ok = Buffer.alloc(text.length, 1);
  let i = 0;
  while (i < text.length) {
    const ch = text[i];
    const next = text[i + 1];
    if (ch === "/" && next === "/") {
      while (i < text.length && text[i] !== "\n") {
        ok[i++] = 0;
      }
      continue;
    }
    if (ch === "/" && next === "*") {
      ok[i++] = 0;
      ok[i++] = 0;
      while (i < text.length) {
        ok[i] = 0;
        if (text[i - 1] === "*" && text[i] === "/") {
          i += 1;
          break;
        }
        i += 1;
      }
      continue;
    }
    if (ch === '"' || ch === "'" || ch === "`") {
      const quote = ch;
      ok[i++] = 0;
      while (i < text.length && text[i] !== quote) {
        if (text[i] === "\\") {
          ok[i++] = 0;
          if (i < text.length) {
            ok[i++] = 0;
          }
          continue;
        }
        if (quote === "`" && text[i] === "{") {
          i += 1;
          while (i < text.length && text[i] !== "}") {
            i += 1;
          }
          if (i < text.length) {
            i += 1;
          }
          continue;
        }
        ok[i++] = 0;
      }
      if (i < text.length) {
        ok[i++] = 0;
      }
      continue;
    }
    i += 1;
  }
  return ok;
}

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
        const symbols = symbolsFor(engine, document);
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
      const symbols = symbolsFor(engine, document);
      const detail = engine.hoverFor(word, symbols);
      if (detail) {
        return new vscode.Hover(detail);
      }
      return null;
    },
  });

  const legend = new vscode.SemanticTokensLegend(["type", "property"], []);
  const semantic = vscode.languages.registerDocumentSemanticTokensProvider(
    selector,
    {
      provideDocumentSemanticTokens(document) {
        const builder = new vscode.SemanticTokensBuilder(legend);
        const symbols = symbolsFor(engine, document);
        const text = document.getText();
        const mask = codeMask(text);
        for (const name of typeNamesFor(data, symbols)) {
          const re = new RegExp(`\\b${name}\\b`, "g");
          let match;
          while ((match = re.exec(text))) {
            if (!mask[match.index]) {
              continue;
            }
            const pos = document.positionAt(match.index);
            builder.push(pos.line, pos.character, name.length, 0);
          }
        }
        const propRe = /\.(?![:.])([A-Za-z_][A-Za-z0-9_]*)|(?<![.:]):(?!:)([A-Za-z_][A-Za-z0-9_]*)/g;
        let prop;
        while ((prop = propRe.exec(text))) {
          const label = prop[1] || prop[2];
          const start = prop.index + (prop[0].length - label.length);
          if (!mask[start]) {
            continue;
          }
          const pos = document.positionAt(start);
          builder.push(pos.line, pos.character, label.length, 1);
        }
        return builder.build();
      },
    },
    legend
  );

  const links = vscode.languages.registerDocumentLinkProvider(selector, {
    provideDocumentLinks(document) {
      const items = [];
      for (const inc of eachInclude(document)) {
        if (!inc.resolved) {
          continue;
        }
        items.push(new vscode.DocumentLink(inc.specRange, vscode.Uri.file(inc.resolved)));
      }
      return items;
    },
  });

  const definitions = vscode.languages.registerDefinitionProvider(selector, {
    provideDefinition(document, position) {
      for (const inc of eachInclude(document)) {
        if (inc.specRange.contains(position) && inc.resolved) {
          return new vscode.Location(vscode.Uri.file(inc.resolved), new vscode.Position(0, 0));
        }
      }
      return [];
    },
  });

  const outline = vscode.languages.registerDocumentSymbolProvider(selector, {
    provideDocumentSymbols(document) {
      return [...includeOutline(engine, document), ...localOutline(engine, document)];
    },
  });

  const collection = vscode.languages.createDiagnosticCollection("clpp");
  const lens = createLens(vscode);
  let timer;
  function refresh(document) {
    if (!document || document.languageId !== "clpp") {
      return;
    }
    const fromCompiler = clppDiagnostics(document);
    const lint = lintDocument(document.getText());
    const issues = mergeIssues(fromCompiler, lint);
    const diagnostics = issues.map((issue) => {
      const line = Math.min(issue.line, Math.max(0, document.lineCount - 1));
      const row = document.lineAt(line);
      const startCol = Math.min(issue.column || 0, Math.max(0, row.text.length));
      const start = new vscode.Position(line, startCol);
      return new vscode.Diagnostic(
        new vscode.Range(start, row.range.end),
        issue.message,
        vscode.DiagnosticSeverity.Error
      );
    });
    collection.set(document.uri, diagnostics);
    applyLensToOpenEditors(vscode, lens, collection);
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
    lens.error,
    vscode.workspace.onDidOpenTextDocument(refresh),
    vscode.workspace.onDidCloseTextDocument((doc) => collection.delete(doc.uri)),
    vscode.window.onDidChangeActiveTextEditor(() => applyLensToOpenEditors(vscode, lens, collection)),
    vscode.workspace.onDidChangeConfiguration((ev) => {
      if (ev.affectsConfiguration("clpp.lens")) {
        applyLensToOpenEditors(vscode, lens, collection);
      }
    })
  );

  context.subscriptions.push(completion, hover, semantic, links, definitions, outline, diagnostics);
  refreshAll();
}

function mergeIssues(compiler, lint) {
  const items = compiler ? [...compiler] : [];
  const seen = new Set(items.map((i) => `${i.line}:${i.message}`));
  for (const issue of lint) {
    const key = `${issue.line}:${issue.message}`;
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    items.push(issue);
  }
  return items;
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
