// CL++ for VS Code — language client.
//
// The language intelligence lives in the compiler (`clpp --lsp`), so the editor, the CLI and the
// build always agree. This file only starts that server and maps Language Server Protocol
// messages to VS Code features. It has no npm dependencies on purpose: the extension works
// offline and installs from a single .vsix.
//
// Server lookup order (first hit wins), same idea as the Zig/Rust/Go extensions:
//   1. setting `clpp.serverPath`
//   2. the binary bundled in this extension: bin/<platform>-<arch>/clpp(.exe)
//   3. `clpp` / `clpp.exe` on PATH
//   4. the installer location: %LOCALAPPDATA%\Programs\CLPP\clpp.exe (Windows) or ~/.local/bin/clpp

"use strict";

const vscode = require("vscode");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawn } = require("child_process");

const LANGUAGE = "clpp";
const REQUEST_TIMEOUT_MS = 10000;

let output;
let statusItem;
let client;

// ---------------------------------------------------------------------------------------------
// Server discovery
// ---------------------------------------------------------------------------------------------

function executableName() {
  return process.platform === "win32" ? "clpp.exe" : "clpp";
}

function onPath() {
  const separator = process.platform === "win32" ? ";" : ":";
  for (const entry of (process.env.PATH || "").split(separator)) {
    if (!entry) {
      continue;
    }
    const candidate = path.join(entry, executableName());
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return undefined;
}

function findServer(context) {
  const configured = vscode.workspace.getConfiguration("clpp").get("serverPath");
  if (configured) {
    const resolved = configured.replace(/^~(?=$|[\\/])/, os.homedir());
    if (fs.existsSync(resolved)) {
      return { path: resolved, source: "setting clpp.serverPath" };
    }
    log("clpp.serverPath does not exist: " + resolved);
  }
  const bundled = context.asAbsolutePath(path.join("bin", `${process.platform}-${process.arch}`, executableName()));
  if (fs.existsSync(bundled)) {
    return { path: bundled, source: "bundled with the extension" };
  }
  const fromPath = onPath();
  if (fromPath) {
    return { path: fromPath, source: "PATH" };
  }
  const installed =
    process.platform === "win32"
      ? path.join(process.env.LOCALAPPDATA || "", "Programs", "CLPP", "clpp.exe")
      : path.join(os.homedir(), ".local", "bin", "clpp");
  if (fs.existsSync(installed)) {
    return { path: installed, source: "installer" };
  }
  return undefined;
}

function log(line) {
  try {
    if (output) {
      output.appendLine(`[${new Date().toLocaleTimeString()}] ${line}`);
    }
  } catch (_) {
    /* output channel already disposed */
  }
}

// ---------------------------------------------------------------------------------------------
// JSON-RPC over stdio
// ---------------------------------------------------------------------------------------------

class Connection {
  constructor(serverPath, onNotification, onExit) {
    this.pending = new Map();
    this.nextId = 1;
    this.buffer = Buffer.alloc(0);
    this.onNotification = onNotification;
    this.process = spawn(serverPath, ["--lsp"], { cwd: path.dirname(serverPath), windowsHide: true });
    this.process.on("error", (error) => {
      log("failed to start: " + error.message);
      this.rejectAll(error);
    });
    this.process.on("exit", (code, signal) => {
      this.rejectAll(new Error("CL++ language server stopped"));
      onExit(code, signal);
    });
    this.process.stdout.on("data", (chunk) => {
      this.buffer = Buffer.concat([this.buffer, chunk]);
      this.drain();
    });
    this.process.stderr.on("data", (chunk) => log(chunk.toString("utf8").trimEnd()));
  }

  rejectAll(error) {
    for (const [, entry] of this.pending) {
      clearTimeout(entry.timer);
      entry.reject(error);
    }
    this.pending.clear();
  }

  drain() {
    for (;;) {
      const headerEnd = this.buffer.indexOf("\r\n\r\n");
      if (headerEnd < 0) {
        return;
      }
      const header = this.buffer.slice(0, headerEnd).toString("ascii");
      const match = /Content-Length:\s*(\d+)/i.exec(header);
      if (!match) {
        this.buffer = this.buffer.slice(headerEnd + 4);
        continue;
      }
      const length = Number(match[1]);
      const start = headerEnd + 4;
      if (this.buffer.length < start + length) {
        return;
      }
      const body = this.buffer.slice(start, start + length).toString("utf8");
      this.buffer = this.buffer.slice(start + length);
      let message;
      try {
        message = JSON.parse(body);
      } catch (error) {
        log("invalid JSON from server: " + error.message);
        continue;
      }
      if (message.id !== undefined && message.id !== null && !message.method) {
        const entry = this.pending.get(message.id);
        if (!entry) {
          continue;
        }
        this.pending.delete(message.id);
        clearTimeout(entry.timer);
        if (message.error) {
          entry.reject(new Error(message.error.message || "request failed"));
        } else {
          entry.resolve(message.result);
        }
      } else if (message.method) {
        this.onNotification(message.method, message.params);
      }
    }
  }

  write(message) {
    if (!this.process || !this.process.stdin.writable) {
      throw new Error("CL++ language server is not running");
    }
    const body = Buffer.from(JSON.stringify(message), "utf8");
    this.process.stdin.write(`Content-Length: ${body.length}\r\n\r\n`);
    this.process.stdin.write(body);
  }

  request(method, params, token) {
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error("CL++ timed out: " + method));
      }, REQUEST_TIMEOUT_MS);
      this.pending.set(id, { resolve, reject, timer });
      if (token) {
        token.onCancellationRequested(() => {
          if (this.pending.has(id)) {
            this.notify("$/cancelRequest", { id });
          }
        });
      }
      try {
        this.write({ jsonrpc: "2.0", id, method, params });
      } catch (error) {
        clearTimeout(timer);
        this.pending.delete(id);
        reject(error);
      }
    });
  }

  notify(method, params) {
    try {
      this.write({ jsonrpc: "2.0", method, params });
    } catch (error) {
      log(String(error.message || error));
    }
  }

  async stop() {
    try {
      await Promise.race([this.request("shutdown", null), new Promise((r) => setTimeout(r, 1500))]);
      this.notify("exit", null);
    } catch (_) {
      /* already gone */
    }
    setTimeout(() => {
      try {
        this.process.kill();
      } catch (_) {
        /* ignore */
      }
    }, 500);
  }
}

// ---------------------------------------------------------------------------------------------
// LSP <-> VS Code conversions
// ---------------------------------------------------------------------------------------------

const toPosition = (p) => new vscode.Position(p.line, p.character);
const toRange = (r) => new vscode.Range(toPosition(r.start), toPosition(r.end));
const fromPosition = (p) => ({ line: p.line, character: p.character });
const fromRange = (r) => ({ start: fromPosition(r.start), end: fromPosition(r.end) });
const docParams = (document) => ({ textDocument: { uri: document.uri.toString() } });
const posParams = (document, position) => ({ ...docParams(document), position: fromPosition(position) });

function toMarkdown(contents) {
  if (!contents) {
    return undefined;
  }
  const value = typeof contents === "string" ? contents : contents.value;
  const markdown = new vscode.MarkdownString(value);
  markdown.supportHtml = false;
  return markdown;
}

function toSeverity(severity) {
  switch (severity) {
    case 2:
      return vscode.DiagnosticSeverity.Warning;
    case 3:
      return vscode.DiagnosticSeverity.Information;
    case 4:
      return vscode.DiagnosticSeverity.Hint;
    default:
      return vscode.DiagnosticSeverity.Error;
  }
}

function toWorkspaceEdit(edit) {
  const result = new vscode.WorkspaceEdit();
  for (const [uri, changes] of Object.entries((edit && edit.changes) || {})) {
    for (const change of changes) {
      result.replace(vscode.Uri.parse(uri), toRange(change.range), change.newText);
    }
  }
  return result;
}

function toSymbol(symbol) {
  const item = new vscode.DocumentSymbol(
    symbol.name,
    symbol.detail || "",
    Math.max(0, (symbol.kind || 13) - 1),
    toRange(symbol.range),
    toRange(symbol.selectionRange || symbol.range)
  );
  item.children = (symbol.children || []).map(toSymbol);
  return item;
}

// ---------------------------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------------------------

class Client {
  constructor(context, serverPath) {
    this.context = context;
    this.serverPath = serverPath;
    this.diagnostics = vscode.languages.createDiagnosticCollection(LANGUAGE);
    this.disposables = [this.diagnostics];
    this.restarts = [];
    this.stopping = false;
    this.ready = undefined;
  }

  start() {
    this.connection = new Connection(
      this.serverPath,
      (method, params) => this.onNotification(method, params),
      (code) => this.onExit(code)
    );
    const folder = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0];
    this.ready = this.connection
      .request("initialize", {
        processId: process.pid,
        clientInfo: { name: "vscode", version: vscode.version },
        rootUri: folder ? folder.uri.toString() : null,
        capabilities: { general: { positionEncodings: ["utf-16"] } },
      })
      .then((result) => {
        this.capabilities = (result && result.capabilities) || {};
        this.connection.notify("initialized", {});
        for (const document of vscode.workspace.textDocuments) {
          this.didOpen(document);
        }
        setStatus("$(check) CL++", "CL++ language server running: " + this.serverPath);
        log("server ready");
      })
      .catch((error) => {
        log("initialize failed: " + error.message);
        setStatus("$(error) CL++", "CL++ language server failed: " + error.message);
      });
    return this.ready;
  }

  onExit(code) {
    this.diagnostics.clear();
    if (this.stopping) {
      return;
    }
    log(`server exited (${code})`);
    const now = Date.now();
    this.restarts = this.restarts.filter((t) => now - t < 3 * 60 * 1000);
    if (this.restarts.length >= 5) {
      setStatus("$(error) CL++", "CL++ language server keeps crashing; see the CL++ output");
      vscode.window.showErrorMessage("The CL++ language server crashed 5 times in 3 minutes. See Output > CL++.");
      return;
    }
    this.restarts.push(now);
    setStatus("$(sync~spin) CL++", "Restarting the CL++ language server");
    this.start();
  }

  onNotification(method, params) {
    if (method === "textDocument/publishDiagnostics") {
      const uri = vscode.Uri.parse(params.uri);
      this.diagnostics.set(
        uri,
        (params.diagnostics || []).map((item) => {
          const diagnostic = new vscode.Diagnostic(toRange(item.range), item.message, toSeverity(item.severity));
          diagnostic.source = item.source || "clpp";
          if (item.code !== undefined) {
            diagnostic.code = item.code;
          }
          return diagnostic;
        })
      );
    } else if (method === "window/logMessage" && params) {
      log(params.message);
    }
  }

  isOurs(document) {
    return document && document.languageId === LANGUAGE && (document.uri.scheme === "file" || document.uri.scheme === "untitled");
  }

  didOpen(document) {
    if (!this.isOurs(document)) {
      return;
    }
    this.connection.notify("textDocument/didOpen", {
      textDocument: { uri: document.uri.toString(), languageId: LANGUAGE, version: document.version, text: document.getText() },
    });
  }

  didChange(event) {
    if (!this.isOurs(event.document) || event.contentChanges.length === 0) {
      return;
    }
    this.connection.notify("textDocument/didChange", {
      textDocument: { uri: event.document.uri.toString(), version: event.document.version },
      contentChanges: [{ text: event.document.getText() }],
    });
  }

  async request(method, params, token) {
    await this.ready;
    try {
      return await this.connection.request(method, params, token);
    } catch (error) {
      log(`${method}: ${error.message}`);
      return undefined;
    }
  }

  register() {
    const selector = [
      { language: LANGUAGE, scheme: "file" },
      { language: LANGUAGE, scheme: "untitled" },
    ];
    const self = this;
    const push = (d) => this.disposables.push(d);

    push(vscode.workspace.onDidOpenTextDocument((d) => this.didOpen(d)));
    push(vscode.workspace.onDidChangeTextDocument((e) => this.didChange(e)));
    push(
      vscode.workspace.onDidSaveTextDocument((d) => {
        if (this.isOurs(d)) {
          this.connection.notify("textDocument/didSave", docParams(d));
        }
      })
    );
    push(
      vscode.workspace.onDidCloseTextDocument((d) => {
        if (this.isOurs(d)) {
          this.connection.notify("textDocument/didClose", docParams(d));
        }
      })
    );
    const watcher = vscode.workspace.createFileSystemWatcher("**/*.clp");
    const changed = (uri, type) =>
      this.connection.notify("workspace/didChangeWatchedFiles", { changes: [{ uri: uri.toString(), type }] });
    push(watcher);
    push(watcher.onDidCreate((u) => changed(u, 1)));
    push(watcher.onDidChange((u) => changed(u, 2)));
    push(watcher.onDidDelete((u) => changed(u, 3)));

    push(
      vscode.languages.registerCompletionItemProvider(
        selector,
        {
          async provideCompletionItems(document, position, token) {
            const result = await self.request("textDocument/completion", posParams(document, position), token);
            const items = Array.isArray(result) ? result : (result && result.items) || [];
            const wordRange = document.getWordRangeAtPosition(position);
            return new vscode.CompletionList(
              items.map((item) => {
                const completion = new vscode.CompletionItem(item.label, Math.max(0, (item.kind || 1) - 1));
                completion.detail = item.detail;
                if (item.documentation) {
                  completion.documentation = toMarkdown(item.documentation);
                }
                completion.sortText = item.sortText;
                completion.filterText = item.filterText || item.label;
                if (item.insertText) {
                  completion.insertText =
                    item.insertTextFormat === 2 ? new vscode.SnippetString(item.insertText) : item.insertText;
                }
                if (wordRange && !item.label.includes("/")) {
                  completion.range = wordRange;
                }
                if (item.command) {
                  completion.command = { title: item.command.title, command: item.command.command };
                }
                return completion;
              }),
              Boolean(result && result.isIncomplete)
            );
          },
        },
        ".",
        "@",
        '"',
        "/",
        ":"
      )
    );
    push(
      vscode.languages.registerHoverProvider(selector, {
        async provideHover(document, position, token) {
          const result = await self.request("textDocument/hover", posParams(document, position), token);
          if (!result || !result.contents) {
            return undefined;
          }
          return new vscode.Hover(toMarkdown(result.contents), result.range ? toRange(result.range) : undefined);
        },
      })
    );
    push(
      vscode.languages.registerSignatureHelpProvider(
        selector,
        {
          async provideSignatureHelp(document, position, token) {
            const result = await self.request("textDocument/signatureHelp", posParams(document, position), token);
            if (!result || !result.signatures || result.signatures.length === 0) {
              return undefined;
            }
            const help = new vscode.SignatureHelp();
            help.signatures = result.signatures.map((signature) => {
              const info = new vscode.SignatureInformation(signature.label, toMarkdown(signature.documentation));
              info.parameters = (signature.parameters || []).map((p) => new vscode.ParameterInformation(p.label));
              return info;
            });
            help.activeSignature = result.activeSignature || 0;
            help.activeParameter = result.activeParameter || 0;
            return help;
          },
        },
        { triggerCharacters: ["(", ","], retriggerCharacters: [","] }
      )
    );
    const locationProvider = (method) => async (document, position, token) => {
      const result = await self.request(method, posParams(document, position), token);
      if (!result) {
        return undefined;
      }
      const list = Array.isArray(result) ? result : [result];
      return list.map((l) => new vscode.Location(vscode.Uri.parse(l.uri), toRange(l.range)));
    };
    push(vscode.languages.registerDefinitionProvider(selector, { provideDefinition: locationProvider("textDocument/definition") }));
    push(
      vscode.languages.registerReferenceProvider(selector, {
        async provideReferences(document, position, context, token) {
          const result = await self.request(
            "textDocument/references",
            { ...posParams(document, position), context: { includeDeclaration: context.includeDeclaration } },
            token
          );
          return (result || []).map((l) => new vscode.Location(vscode.Uri.parse(l.uri), toRange(l.range)));
        },
      })
    );
    push(
      vscode.languages.registerDocumentHighlightProvider(selector, {
        async provideDocumentHighlights(document, position, token) {
          const result = await self.request("textDocument/documentHighlight", posParams(document, position), token);
          return (result || []).map((h) => new vscode.DocumentHighlight(toRange(h.range), Math.max(0, (h.kind || 1) - 1)));
        },
      })
    );
    push(
      vscode.languages.registerRenameProvider(selector, {
        async prepareRename(document, position, token) {
          const result = await self.request("textDocument/prepareRename", posParams(document, position), token);
          if (!result) {
            throw new Error("Only variables and parameters can be renamed here.");
          }
          return { range: toRange(result.range), placeholder: result.placeholder };
        },
        async provideRenameEdits(document, position, newName, token) {
          const result = await self.request("textDocument/rename", { ...posParams(document, position), newName }, token);
          return result ? toWorkspaceEdit(result) : undefined;
        },
      })
    );
    push(
      vscode.languages.registerDocumentSymbolProvider(selector, {
        async provideDocumentSymbols(document, token) {
          const result = await self.request("textDocument/documentSymbol", docParams(document), token);
          return (result || []).map(toSymbol);
        },
      })
    );
    push(
      vscode.languages.registerFoldingRangeProvider(selector, {
        async provideFoldingRanges(document, context, token) {
          const result = await self.request("textDocument/foldingRange", docParams(document), token);
          return (result || []).map((r) => new vscode.FoldingRange(r.startLine, r.endLine));
        },
      })
    );
    push(
      vscode.languages.registerDocumentFormattingEditProvider(selector, {
        async provideDocumentFormattingEdits(document, options, token) {
          const result = await self.request(
            "textDocument/formatting",
            { ...docParams(document), options: { tabSize: options.tabSize, insertSpaces: options.insertSpaces } },
            token
          );
          return (result || []).map((edit) => new vscode.TextEdit(toRange(edit.range), edit.newText));
        },
      })
    );
    push(
      vscode.languages.registerCodeActionsProvider(
        selector,
        {
          async provideCodeActions(document, range, context, token) {
            const diagnostics = context.diagnostics.map((d) => ({
              range: fromRange(d.range),
              message: d.message,
              severity: d.severity + 1,
              source: d.source,
            }));
            const result = await self.request(
              "textDocument/codeAction",
              { ...docParams(document), range: fromRange(range), context: { diagnostics } },
              token
            );
            return (result || []).map((action) => {
              const item = new vscode.CodeAction(action.title, vscode.CodeActionKind.QuickFix);
              item.edit = toWorkspaceEdit(action.edit);
              item.isPreferred = Boolean(action.isPreferred);
              item.diagnostics = context.diagnostics;
              return item;
            });
          },
        },
        { providedCodeActionKinds: [vscode.CodeActionKind.QuickFix] }
      )
    );
    // Must match semantic_token_types()/semantic_token_modifiers() in the server (src/core/ide/ide.cpp).
    const legend = new vscode.SemanticTokensLegend(
      ["type", "struct", "enum", "enumMember", "parameter", "variable", "property", "function", "keyword",
       "namespace", "method", "typeParameter"],
      ["declaration", "readonly", "async", "defaultLibrary", "static"]
    );
    push(
      vscode.languages.registerDocumentSemanticTokensProvider(
        selector,
        {
          async provideDocumentSemanticTokens(document, token) {
            const result = await self.request("textDocument/semanticTokens/full", docParams(document), token);
            return new vscode.SemanticTokens(new Uint32Array((result && result.data) || []));
          },
        },
        legend
      )
    );
  }

  async stop() {
    this.stopping = true;
    for (const d of this.disposables) {
      try {
        d.dispose();
      } catch (_) {
        /* ignore */
      }
    }
    this.disposables = [];
    if (this.connection) {
      await this.connection.stop();
    }
  }
}

function setStatus(text, tooltip) {
  if (!statusItem) {
    return;
  }
  statusItem.text = text;
  statusItem.tooltip = tooltip;
  statusItem.show();
}

// ---------------------------------------------------------------------------------------------
// Color decorators (Gfx.Rgb / Gfx.Hsv / Gfx.Hex / 0xRRGGBB…)
// ---------------------------------------------------------------------------------------------

function parseNum(text) {
  const cleaned = text.replace(/_/g, "");
  const value = Number(cleaned);
  return Number.isFinite(value) ? value : 0;
}

function clampByte(value) {
  return Math.min(255, Math.max(0, Math.round(value)));
}

function colorFromRgbBytes(r, g, b, alpha255) {
  const alpha = alpha255 == null ? 1 : clampByte(alpha255) / 255;
  return new vscode.Color(r / 255, g / 255, b / 255, alpha);
}

function colorFromPacked(packed) {
  const value = packed >>> 0;
  const rgb = value & 0xffffff;
  const transparency = (value >>> 24) & 0xff;
  const alpha = (255 - transparency) / 255;
  return new vscode.Color(((rgb >>> 16) & 0xff) / 255, ((rgb >>> 8) & 0xff) / 255, (rgb & 0xff) / 255, alpha);
}

function hsvToColor(h, s, v) {
  let hue = h % 360;
  if (hue < 0) {
    hue += 360;
  }
  s = Math.min(1, Math.max(0, s));
  v = Math.min(1, Math.max(0, v));
  const c = v * s;
  const x = c * (1 - Math.abs(((hue / 60) % 2) - 1));
  const m = v - c;
  let r = 0;
  let g = 0;
  let b = 0;
  if (hue < 60) {
    r = c;
    g = x;
  } else if (hue < 120) {
    r = x;
    g = c;
  } else if (hue < 180) {
    g = c;
    b = x;
  } else if (hue < 240) {
    g = x;
    b = c;
  } else if (hue < 300) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }
  const ch = (value) => clampByte((value + m) * 255);
  return colorFromRgbBytes(ch(r), ch(g), ch(b));
}

function parseGfxHexString(raw) {
  let text = raw;
  if ((text.startsWith('"') && text.endsWith('"')) || (text.startsWith("'") && text.endsWith("'"))) {
    text = text.slice(1, -1);
  }
  if (text.startsWith("#")) {
    text = text.slice(1);
  }
  if (text.length === 3) {
    text = text[0] + text[0] + text[1] + text[1] + text[2] + text[2];
  }
  if (text.length !== 6 || /[^0-9a-fA-F]/.test(text)) {
    return undefined;
  }
  return colorFromPacked(parseInt(text, 16));
}

function hexLiteralColor(raw) {
  const digits = raw.replace(/^0[xX]/, "").replace(/_/g, "");
  if (digits.length !== 6 && digits.length !== 8) {
    return undefined;
  }
  if (!/^[0-9A-Fa-f]+$/.test(digits)) {
    return undefined;
  }
  return colorFromPacked(parseInt(digits, 16));
}

function pushMatch(colors, document, text, match, color) {
  if (!color) {
    return;
  }
  const start = document.positionAt(match.index);
  const end = document.positionAt(match.index + match[0].length);
  colors.push(new vscode.ColorInformation(new vscode.Range(start, end), color));
}

function collectDocumentColors(document) {
  const text = document.getText();
  const colors = [];

  const rgbRe = /\bGfx\.Rgb\s*\(\s*(\d[\d_]*)\s*,\s*(\d[\d_]*)\s*,\s*(\d[\d_]*)(?:\s*,\s*(\d[\d_]*))?\s*\)/g;
  for (let match = rgbRe.exec(text); match; match = rgbRe.exec(text)) {
    const r = clampByte(parseNum(match[1]));
    const g = clampByte(parseNum(match[2]));
    const b = clampByte(parseNum(match[3]));
    const a = match[4] != null ? clampByte(parseNum(match[4])) : null;
    pushMatch(colors, document, text, match, colorFromRgbBytes(r, g, b, a));
  }

  const hsvRe = /\bGfx\.Hsv\s*\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*\)/g;
  for (let match = hsvRe.exec(text); match; match = hsvRe.exec(text)) {
    pushMatch(colors, document, text, match, hsvToColor(parseNum(match[1]), parseNum(match[2]), parseNum(match[3])));
  }

  const hexCallRe = /\bGfx\.Hex\s*\(\s*("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')\s*\)/g;
  for (let match = hexCallRe.exec(text); match; match = hexCallRe.exec(text)) {
    pushMatch(colors, document, text, match, parseGfxHexString(match[1]));
  }

  const litRe = /\b0[xX][0-9A-Fa-f_]+\b/g;
  for (let match = litRe.exec(text); match; match = litRe.exec(text)) {
    pushMatch(colors, document, text, match, hexLiteralColor(match[0]));
  }

  return colors;
}

function colorPresentations(color) {
  const r = clampByte(color.red * 255);
  const g = clampByte(color.green * 255);
  const b = clampByte(color.blue * 255);
  const rgbHex = ((r << 16) | (g << 8) | b) >>> 0;
  const hex6 = rgbHex.toString(16).toUpperCase().padStart(6, "0");
  const transparency = clampByte((1 - color.alpha) * 255);
  const packed = (transparency << 24) | rgbHex;
  const hex8 = (packed >>> 0).toString(16).toUpperCase().padStart(8, "0");
  const out = [
    new vscode.ColorPresentation(`0x${hex6}`, `CL++ color 0x${hex6}`),
    new vscode.ColorPresentation(`Gfx.Rgb(${r}, ${g}, ${b})`, "Gfx.Rgb"),
    new vscode.ColorPresentation(`#${hex6}`, "CSS hex"),
  ];
  if (color.alpha < 0.999) {
    out.splice(1, 0, new vscode.ColorPresentation(`0x${hex8}`, `CL++ color with alpha 0x${hex8}`));
    out.push(new vscode.ColorPresentation(`Gfx.Rgb(${r}, ${g}, ${b}, ${clampByte(color.alpha * 255)})`, "Gfx.Rgb + alpha"));
  }
  return out;
}

function registerClppColorProvider() {
  return vscode.languages.registerColorProvider(LANGUAGE, {
    provideDocumentColors(document) {
      return collectDocumentColors(document);
    },
    provideColorPresentations(color) {
      return colorPresentations(color);
    },
  });
}

async function startClient(context) {
  const server = findServer(context);
  if (!server) {
    setStatus("$(warning) CL++", "clpp not found");
    const choice = await vscode.window.showErrorMessage(
      "CL++: the compiler (clpp) was not found. Install CL++ or set 'clpp.serverPath'.",
      "Open settings"
    );
    if (choice === "Open settings") {
      vscode.commands.executeCommand("workbench.action.openSettings", "clpp.serverPath");
    }
    return;
  }
  log(`starting ${server.path} (${server.source})`);
  client = new Client(context, server.path);
  client.register();
  await client.start();
}

async function activate(context) {
  output = vscode.window.createOutputChannel("CL++");
  statusItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 0);
  statusItem.command = "clpp.showOutput";
  context.subscriptions.push(output, statusItem, registerClppColorProvider());
  context.subscriptions.push(
    vscode.commands.registerCommand("clpp.showOutput", () => output.show(true)),
    vscode.commands.registerCommand("clpp.restartServer", async () => {
      if (client) {
        await client.stop();
      }
      await startClient(context);
    }),
    vscode.commands.registerCommand("clpp.runFile", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor || editor.document.languageId !== LANGUAGE) {
        return;
      }
      await editor.document.save();
      const server = findServer(context);
      if (!server) {
        return;
      }
      const terminal = vscode.window.terminals.find((t) => t.name === "CL++") || vscode.window.createTerminal("CL++");
      terminal.show(true);
      const quote = (p) => (process.platform === "win32" ? `& "${p}"` : `"${p}"`);
      terminal.sendText(`${quote(server.path)} "${editor.document.uri.fsPath}"`);
    }),
    vscode.workspace.onDidChangeConfiguration(async (event) => {
      if (event.affectsConfiguration("clpp.serverPath")) {
        if (client) {
          await client.stop();
        }
        await startClient(context);
      }
    })
  );
  await startClient(context);
}

async function deactivate() {
  if (client) {
    await client.stop();
  }
}

module.exports = { activate, deactivate };
