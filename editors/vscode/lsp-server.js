"use strict";

const fs = require("fs");
const path = require("path");
const { Worker } = require("worker_threads");
const { createFramer, writeMessage } = require("./jsonrpc");
const { loadEngine, lintDocument } = require("./intellisense");
const { compileDiagnostics, compileDiagnosticsAsync, mergeIssues } = require("./compile-api");
const { buildCompletionItems, hoverText } = require("./complete");
const { extrasFor, scheduleLoad } = require("./include-cache");
const {
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
  let indexWorker = null;
  const timers = new Map();

  function setFolders(next) {
    folders = (next || []).filter(Boolean);
  }

  function startIndexWorker() {
    methods = [];
    if (indexWorker) {
      indexWorker.terminate().catch(() => {});
      indexWorker = null;
    }
    if (!folders.length) {
      return;
    }
    const roots = folders.slice();
    try {
      indexWorker = new Worker(path.join(__dirname, "index-worker.js"), {
        workerData: { folders: roots },
      });
      indexWorker.on("message", (result) => {
        methods = Array.isArray(result) ? result : [];
        if (indexWorker) {
          indexWorker.terminate().catch(() => {});
          indexWorker = null;
        }
      });
      indexWorker.on("error", () => {
        indexWorker = null;
        setImmediate(() => {
          methods = indexMethods(roots);
        });
      });
    } catch {
      setImmediate(() => {
        methods = indexMethods(roots);
      });
    }
  }

  function lineAt(text, position) {
    const lines = text.split(/\r?\n/);
    return (lines[position.line] || "").slice(0, position.character || 0);
  }

  function getDoc(uri) {
    return documents.get(uri) || null;
  }

  function collectDiagnosticsSync(doc) {
    const compiler = compileDiagnostics(doc.text, doc.filePath || "untitled.clpp", folders);
    const lint = lintDocument(doc.text);
    return mergeIssues(compiler, lint);
  }

  function publishDiagnostics(uri, issues, send) {
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
            completionProvider: { triggerCharacters: [".", ":", ">", "@", "\"", "/"] },
            hoverProvider: true,
            definitionProvider: true,
            workspace: { workspaceFolders: { supported: true, changeNotifications: true } },
          },
        },
      });
      return;
    }
    if (method === "initialized") {
      startIndexWorker();
      return;
    }
    if (method === "workspace/didChangeWorkspaceFolders") {
      const added = ((params && params.event && params.event.added) || [])
        .map((f) => uriToPath(f.uri))
        .filter(Boolean);
      const removed = new Set(
        ((params && params.event && params.event.removed) || [])
          .map((f) => uriToPath(f.uri))
          .filter(Boolean)
      );
      const next = folders.filter((folder) => !removed.has(folder)).concat(added);
      setFolders(next);
      startIndexWorker();
      return;
    }
    if (method === "shutdown") {
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
      scheduleLoad(doc.text || "", uriToPath(doc.uri), folders);
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
      scheduleLoad(current.text, current.filePath, folders);
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
        folders,
        extrasFor(doc.filePath)
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
        folders,
        extrasFor(doc.filePath)
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
        const lint = lintDocument(doc.text);
        compileDiagnosticsAsync(doc.text, doc.filePath || "untitled.clpp", folders, (compiler) => {
          if (!documents.has(uri)) {
            return;
          }
          publishDiagnostics(uri, mergeIssues(compiler, lint), send);
        });
      }, 250)
    );
  }

  return {
    handle,
    setFolders,
    documents,
    engine,
    extrasFor: (filePath) => extrasFor(filePath),
    collectDiagnostics: collectDiagnosticsSync,
    flushDiagnostics(uri, send) {
      const doc = documents.get(uri);
      if (!doc) {
        return [];
      }
      const issues = collectDiagnosticsSync(doc);
      if (send) {
        publishDiagnostics(uri, issues, send);
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
    File: 17,
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
