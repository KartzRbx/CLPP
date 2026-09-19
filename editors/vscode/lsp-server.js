"use strict";

const fs = require("fs");
const path = require("path");
const { createFramer, writeMessage } = require("./jsonrpc");
const { loadEngine, lintDocument } = require("./intellisense");
const { compileDiagnostics, mergeIssues } = require("./compile-api");
const { buildCompletionItems, hoverText } = require("./complete");
const {
  extraIncludeTexts,
  findDefinitions,
  indexMethods,
  wordAt,
  uriToPath,
  positionToOffset,
} = require("./workspace-index");

function loadCompletions() {
  const file = path.join(__dirname, "data", "completions.json");
  try {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  } catch {
    return { keywords: [], builtins: [], types: [], operators: [], catalog: {}, globals: {} };
  }
}

function createSession() {
  const data = loadCompletions();
  const engine = loadEngine(data);
  const documents = new Map();
  let folders = [];
  let methods = [];
  const timers = new Map();

  function setFolders(next) {
    folders = (next || []).filter(Boolean);
    methods = indexMethods(folders);
  }

  function lineAt(text, position) {
    const lines = text.split(/\r?\n/);
    return (lines[position.line] || "").slice(0, position.character || 0);
  }

  function getDoc(uri) {
    return documents.get(uri) || null;
  }

  function collectDiagnostics(doc) {
    const compiler = compileDiagnostics(doc.text, doc.filePath || "untitled.clpp", folders);
    const lint = lintDocument(doc.text);
    return mergeIssues(compiler, lint);
  }

  function handle(message, send) {
    if (!message || typeof message !== "object") {
      return;
    }
    const { id, method, params } = message;
    if (method === "initialize") {
      const wf = (params && params.workspaceFolders) || [];
      const roots = wf.map((f) => uriToPath(f.uri)).filter(Boolean);
      if (!roots.length && params && params.rootUri) {
        const root = uriToPath(params.rootUri);
        if (root) {
          roots.push(root);
        }
      }
      setFolders(roots);
      send({
        jsonrpc: "2.0",
        id,
        result: {
          capabilities: {
            textDocumentSync: 1,
            completionProvider: { triggerCharacters: [".", ":", ">", "@"] },
            hoverProvider: true,
            definitionProvider: true,
          },
        },
      });
      return;
    }
    if (method === "initialized" || method === "shutdown") {
      if (id !== undefined) {
        send({ jsonrpc: "2.0", id, result: null });
      }
      return;
    }
    if (method === "exit") {
      process.exit(0);
    }
    if (method === "textDocument/didOpen") {
      const doc = params.textDocument;
      documents.set(doc.uri, {
        uri: doc.uri,
        text: doc.text || "",
        filePath: uriToPath(doc.uri),
      });
      scheduleDiagnostics(doc.uri, send);
      return;
    }
    if (method === "textDocument/didChange") {
      const uri = params.textDocument.uri;
      const current = documents.get(uri) || { uri, filePath: uriToPath(uri), text: "" };
      const change = (params.contentChanges || [])[0];
      if (change && typeof change.text === "string") {
        current.text = change.text;
      }
      documents.set(uri, current);
      scheduleDiagnostics(uri, send);
      return;
    }
    if (method === "textDocument/didClose") {
      documents.delete(params.textDocument.uri);
      const uri = params.textDocument.uri;
      if (timers.has(uri)) {
        clearTimeout(timers.get(uri));
        timers.delete(uri);
      }
      send({
        jsonrpc: "2.0",
        method: "textDocument/publishDiagnostics",
        params: { uri, diagnostics: [] },
      });
      return;
    }
    if (method === "textDocument/completion") {
      const doc = getDoc(params.textDocument.uri);
      if (!doc) {
        send({ jsonrpc: "2.0", id, result: [] });
        return;
      }
      const lineText = lineAt(doc.text, params.position);
      const offset = positionToOffset(doc.text, params.position);
      const items = buildCompletionItems(
        engine,
        data,
        doc.text,
        doc.filePath,
        lineText,
        offset,
        folders
      );
      send({
        jsonrpc: "2.0",
        id,
        result: items.map((item) => ({
          label: item.label,
          detail: item.detail || "",
          kind: lspCompletionKind(item.kind),
        })),
      });
      return;
    }
    if (method === "textDocument/hover") {
      const doc = getDoc(params.textDocument.uri);
      if (!doc) {
        send({ jsonrpc: "2.0", id, result: null });
        return;
      }
      const hit = wordAt(doc.text, params.position);
      const detail = hoverText(
        engine,
        doc.text,
        doc.filePath,
        hit && hit.word,
        folders
      );
      send({
        jsonrpc: "2.0",
        id,
        result: detail
          ? { contents: { kind: "markdown", value: detail } }
          : null,
      });
      return;
    }
    if (method === "textDocument/definition") {
      const doc = getDoc(params.textDocument.uri);
      if (!doc) {
        send({ jsonrpc: "2.0", id, result: [] });
        return;
      }
      send({
        jsonrpc: "2.0",
        id,
        result: findDefinitions(doc.text, doc.filePath, params.position, folders, methods),
      });
      return;
    }
    if (id !== undefined) {
      send({ jsonrpc: "2.0", id, result: null });
    }
  }

  function scheduleDiagnostics(uri, send) {
    if (timers.has(uri)) {
      clearTimeout(timers.get(uri));
    }
    timers.set(
      uri,
      setTimeout(() => {
        timers.delete(uri);
        const doc = documents.get(uri);
        if (!doc) {
          return;
        }
        const issues = collectDiagnostics(doc);
        send({
          jsonrpc: "2.0",
          method: "textDocument/publishDiagnostics",
          params: {
            uri,
            diagnostics: issues.map((issue) => ({
              range: {
                start: { line: issue.line, character: issue.column || 0 },
                end: { line: issue.line, character: (issue.column || 0) + 1 },
              },
              message: issue.message,
              severity: 1,
              source: "clpp",
            })),
          },
        });
      }, 350)
    );
  }

  return {
    handle,
    setFolders,
    documents,
    engine,
    extraIncludeTexts: (text, filePath) => extraIncludeTexts(text, filePath, folders),
    collectDiagnostics,
    flushDiagnostics(uri, send) {
      const doc = documents.get(uri);
      if (!doc) {
        return [];
      }
      const issues = collectDiagnostics(doc);
      if (send) {
        send({
          jsonrpc: "2.0",
          method: "textDocument/publishDiagnostics",
          params: {
            uri,
            diagnostics: issues.map((issue) => ({
              range: {
                start: { line: issue.line, character: issue.column || 0 },
                end: { line: issue.line, character: (issue.column || 0) + 1 },
              },
              message: issue.message,
              severity: 1,
              source: "clpp",
            })),
          },
        });
      }
      return issues;
    },
  };
}

function lspCompletionKind(kind) {
  const map = {
    Keyword: 14,
    Function: 3,
    Method: 2,
    Property: 10,
    Class: 7,
    Operator: 24,
    Variable: 6,
    Event: 23,
    Field: 5,
    tableKeys: 5,
    properties: 10,
    methods: 2,
    events: 23,
  };
  return map[kind] || 1;
}

function startStdio() {
  const session = createSession();
  const send = (msg) => writeMessage(process.stdout, msg);
  const framer = createFramer((message) => session.handle(message, send));
  process.stdin.on("data", (chunk) => framer.push(chunk));
  process.stdin.on("end", () => process.exit(0));
}

if (require.main === module) {
  startStdio();
}

module.exports = { createSession, startStdio };
