const vscode = require("vscode");
const path = require("path");
const fs = require("fs");
const { loadEngine, lintDocument } = require("./intellisense");
const { createLens, applyLensToOpenEditors } = require("./lens");
const { buildCompletionItems, hoverText } = require("./complete");
const { compileDiagnostics, mergeIssues } = require("./compile-api");
const { startLspClient } = require("./lsp-client");

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
          : member.kind === "Keyword"
            ? "Keyword"
            : member.kind === "Function"
              ? "Function"
              : member.kind === "Class"
                ? "Class"
                : member.kind === "Operator"
                  ? "Operator"
                  : member.kind === "Variable"
                    ? "Variable"
                    : member.kind === "Method"
                      ? "Method"
                      : member.kind === "Property"
                        ? "Property"
                        : "Property";
  const sort =
    member.kind === "tableKeys" || member.kind === "properties" || member.kind === "Variable"
      ? "0_"
      : member.kind === "events"
        ? "1_"
        : "2_";
  return item(member.label, kind, member.detail, sort);
}

function workspaceFolders() {
  return (vscode.workspace.workspaceFolders || []).map((folder) => folder.uri.fsPath);
}

function filePathOf(document) {
  return document.uri.scheme === "file" ? document.uri.fsPath : null;
}

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

const WORD_RE = /@[A-Za-z_][A-Za-z0-9_]*|[A-Za-z_][A-Za-z0-9_]*/;

function wireLens(context, lens, collection) {
  context.subscriptions.push(
    lens.error,
    vscode.window.onDidChangeActiveTextEditor(() => applyLensToOpenEditors(vscode, lens, collection)),
    vscode.workspace.onDidChangeConfiguration((ev) => {
      if (ev.affectsConfiguration("clpp.lens")) {
        applyLensToOpenEditors(vscode, lens, collection);
      }
    }),
    vscode.languages.onDidChangeDiagnostics(() => applyLensToOpenEditors(vscode, lens, collection))
  );
  applyLensToOpenEditors(vscode, lens, collection);
}

function registerChrome(context, engine, selector) {
  const legend = new vscode.SemanticTokensLegend(["type", "property"], []);
  const semantic = vscode.languages.registerDocumentSemanticTokensProvider(
    selector,
    {
      provideDocumentSemanticTokens(document) {
        const builder = new vscode.SemanticTokensBuilder(legend);
        const data = loadCompletions();
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
  const outline = vscode.languages.registerDocumentSymbolProvider(selector, {
    provideDocumentSymbols(document) {
      return [...includeOutline(engine, document), ...localOutline(engine, document)];
    },
  });
  context.subscriptions.push(semantic, links, outline);
}

function registerInProcessIntelligence(context, engine, data, selector, lens) {
  const completion = vscode.languages.registerCompletionItemProvider(
    selector,
    {
      provideCompletionItems(document, position) {
        const line = document.lineAt(position).text.slice(0, position.character);
        const raw = buildCompletionItems(
          engine,
          data,
          document.getText(),
          filePathOf(document),
          line,
          document.offsetAt(position),
          workspaceFolders()
        );
        return raw.map(memberItem);
      },
    },
    ".",
    ":",
    ">",
    "@"
  );

  const hover = vscode.languages.registerHoverProvider(selector, {
    provideHover(document, position) {
      const range = document.getWordRangeAtPosition(position, WORD_RE);
      if (!range) {
        return null;
      }
      const detail = hoverText(
        engine,
        document.getText(),
        filePathOf(document),
        document.getText(range),
        workspaceFolders()
      );
      return detail ? new vscode.Hover(detail) : null;
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

  const collection = vscode.languages.createDiagnosticCollection("clpp");
  let timer;
  function refresh(document) {
    if (!document || document.languageId !== "clpp") {
      return;
    }
    const fromCompiler = compileDiagnostics(document.getText(), document.fileName, workspaceFolders());
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
    completion,
    hover,
    definitions,
    diagnostics,
    vscode.workspace.onDidOpenTextDocument(refresh),
    vscode.workspace.onDidCloseTextDocument((doc) => collection.delete(doc.uri))
  );
  wireLens(context, lens, collection);
  refreshAll();
}

function lspKindName(kind) {
  const map = {
    2: "Method",
    3: "Function",
    5: "Field",
    6: "Variable",
    7: "Class",
    10: "Property",
    14: "Keyword",
    23: "Event",
    24: "Operator",
  };
  return map[kind] || "Text";
}

function registerLspIntelligence(context, client, selector, lens) {
  const completion = vscode.languages.registerCompletionItemProvider(
    selector,
    {
      provideCompletionItems(document, position) {
        return client.completion(document, position).then((result) =>
          (result || []).map((entry) => item(entry.label, lspKindName(entry.kind), entry.detail))
        );
      },
    },
    ".",
    ":",
    ">",
    "@"
  );
  const hover = vscode.languages.registerHoverProvider(selector, {
    provideHover(document, position) {
      return client.hover(document, position).then((result) => {
        if (!result || !result.contents) {
          return null;
        }
        const value =
          typeof result.contents === "string"
            ? result.contents
            : result.contents.value || result.contents;
        return value ? new vscode.Hover(String(value)) : null;
      });
    },
  });
  const definitions = vscode.languages.registerDefinitionProvider(selector, {
    provideDefinition(document, position) {
      return client.definition(document, position).then((result) => {
        const list = Array.isArray(result) ? result : result ? [result] : [];
        return list.map(
          (loc) =>
            new vscode.Location(
              vscode.Uri.parse(loc.uri),
              new vscode.Range(
                loc.range.start.line,
                loc.range.start.character,
                loc.range.end.line,
                loc.range.end.character
              )
            )
        );
      });
    },
  });
  context.subscriptions.push(
    completion,
    hover,
    definitions,
    vscode.workspace.onDidOpenTextDocument((doc) => client.open(doc)),
    vscode.workspace.onDidChangeTextDocument((ev) => client.change(ev.document)),
    vscode.workspace.onDidCloseTextDocument((doc) => client.close(doc))
  );
  for (const doc of vscode.workspace.textDocuments) {
    client.open(doc);
  }
  wireLens(context, lens, client.collection);
}

function activate(context) {
  const data = loadCompletions();
  const engine = loadEngine(data);
  const selector = [
    { language: "clpp", scheme: "file" },
    { language: "clpp", scheme: "untitled" },
  ];
  const lens = createLens(vscode);
  registerChrome(context, engine, selector);

  const useLsp = vscode.workspace.getConfiguration("clpp").get("lsp.enabled", true);
  if (useLsp) {
    const client = startLspClient(context, vscode);
    client.ready.then((ok) => {
      if (ok && client.isAlive()) {
        registerLspIntelligence(context, client, selector, lens);
      } else {
        registerInProcessIntelligence(context, engine, data, selector, lens);
      }
    });
    return;
  }
  registerInProcessIntelligence(context, engine, data, selector, lens);
}

function deactivate() {}

module.exports = { activate, deactivate };
