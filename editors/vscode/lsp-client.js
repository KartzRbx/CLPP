"use strict";

const cp = require("child_process");
const { createFramer, writeMessage } = require("./jsonrpc");

function startLspClient(context, vscode) {
  try {
    const { LanguageClient } = require("vscode-languageclient/node");
    const { findClpp } = require("./compile-api");
    const command = findClpp() || "clpp";
    const client = new LanguageClient(
      "clpp",
      "CL++",
      { command, args: ["lsp"] },
      {
        documentSelector: [
          { language: "clpp" },
          { language: "clpp-header" },
        ],
        synchronize: {
          fileEvents: vscode.workspace.createFileSystemWatcher("**/*.{clpp,clp,clh}"),
        },
      }
    );
    const started = client.start();
    context.subscriptions.push({
      dispose() {
        client.stop();
      },
    });
    return {
      ready: Promise.resolve(started).then(() => true).catch(() => false),
      isAlive: () => true,
      collection: vscode.languages.createDiagnosticCollection("clpp"),
      open() {},
      change() {},
      close() {},
      completion() {
        return Promise.resolve({ items: [] });
      },
      hover() {
        return Promise.resolve(null);
      },
      definition() {
        return Promise.resolve(null);
      },
    };
  } catch {
    return startStdioFallback(context, vscode);
  }
}

function startStdioFallback(context, vscode) {
  const { findClpp } = require("./compile-api");
  const clpp = findClpp();
  const child = cp.spawn(clpp || "clpp", ["lsp"], {
    stdio: ["pipe", "pipe", "pipe"],
    windowsHide: true,
  });

  const pending = new Map();
  let nextId = 1;
  let alive = true;
  const collection = vscode.languages.createDiagnosticCollection("clpp");

  child.on("error", (err) => {
    alive = false;
    for (const [, entry] of pending) {
      clearTimeout(entry.timer);
      entry.reject(err);
    }
    pending.clear();
  });
  child.on("exit", () => {
    alive = false;
    for (const [, entry] of pending) {
      clearTimeout(entry.timer);
      entry.reject(new Error("lsp exit"));
    }
    pending.clear();
  });

  const framer = createFramer((message) => {
    if (message.method === "textDocument/publishDiagnostics" && message.params) {
      const uri = vscode.Uri.parse(message.params.uri);
      const diagnostics = (message.params.diagnostics || []).map((d) => {
        const start = new vscode.Position(d.range.start.line, d.range.start.character);
        const end = new vscode.Position(d.range.end.line, d.range.end.character);
        return new vscode.Diagnostic(
          new vscode.Range(start, end),
          d.message,
          vscode.DiagnosticSeverity.Error
        );
      });
      collection.set(uri, diagnostics);
      return;
    }
    if (message.id === undefined || !pending.has(message.id)) {
      return;
    }
    const entry = pending.get(message.id);
    pending.delete(message.id);
    clearTimeout(entry.timer);
    if (message.error) {
      entry.reject(new Error(message.error.message || "lsp error"));
    } else {
      entry.resolve(message.result);
    }
  });
  child.stdout.on("data", (chunk) => framer.push(chunk));

  function send(method, params) {
    if (!alive) {
      return;
    }
    writeMessage(child.stdin, { jsonrpc: "2.0", method, params });
  }

  function request(method, params) {
    if (!alive) {
      return Promise.reject(new Error("lsp down"));
    }
    const id = nextId++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        pending.delete(id);
        reject(new Error("lsp timeout"));
      }, 8000);
      pending.set(id, { resolve, reject, timer });
      writeMessage(child.stdin, { jsonrpc: "2.0", id, method, params });
    });
  }

  const folders = (vscode.workspace.workspaceFolders || []).map((f) => ({
    uri: f.uri.toString(),
    name: f.name,
  }));
  const rootUri = folders[0] ? folders[0].uri : null;

  const ready = request("initialize", {
    processId: process.pid,
    rootUri,
    capabilities: {},
    workspaceFolders: folders,
  })
    .then(() => {
      send("initialized", {});
      return true;
    })
    .catch(() => {
      alive = false;
      try {
        child.kill();
      } catch {
        // ignore
      }
      return false;
    });

  function sync(document) {
    if (document.languageId !== "clpp" && document.languageId !== "clpp-header") {
      return;
    }
    send("textDocument/didChange", {
      textDocument: { uri: document.uri.toString(), version: document.version },
      contentChanges: [{ text: document.getText() }],
    });
  }

  context.subscriptions.push(collection);
  context.subscriptions.push({
    dispose() {
      try {
        send("shutdown");
        child.kill();
      } catch {
        // ignore
      }
    },
  });

  return {
    collection,
    ready,
    isAlive: () => alive,
    open(document) {
      if (document.languageId !== "clpp" && document.languageId !== "clpp-header") {
        return;
      }
      send("textDocument/didOpen", {
        textDocument: {
          uri: document.uri.toString(),
          languageId: "clpp",
          version: document.version,
          text: document.getText(),
        },
      });
    },
    change: sync,
    close(document) {
      send("textDocument/didClose", {
        textDocument: { uri: document.uri.toString() },
      });
    },
    completion(document, position) {
      return request("textDocument/completion", {
        textDocument: { uri: document.uri.toString() },
        position: { line: position.line, character: position.character },
      });
    },
    hover(document, position) {
      return request("textDocument/hover", {
        textDocument: { uri: document.uri.toString() },
        position: { line: position.line, character: position.character },
      });
    },
    definition(document, position) {
      return request("textDocument/definition", {
        textDocument: { uri: document.uri.toString() },
        position: { line: position.line, character: position.character },
      });
    },
  };
}

module.exports = { startLspClient };
