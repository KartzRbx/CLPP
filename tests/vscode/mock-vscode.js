// Minimal stand-in for the `vscode` module: enough of the API for tools/vscode/extension.js to
// activate, register its providers and talk to the real `clpp --lsp`. The test then calls the
// providers exactly as VS Code would.
"use strict";
const path = require("path");
const fs = require("fs");

class Position { constructor(line, character) { this.line = line; this.character = character; } }
class Range {
  constructor(a, b, c, d) {
    if (a instanceof Position) { this.start = a; this.end = b; } else { this.start = new Position(a, b); this.end = new Position(c, d); }
  }
}
class Uri {
  constructor(scheme, fsPath) { this.scheme = scheme; this.fsPath = fsPath; }
  static file(p) { return new Uri("file", p); }
  static parse(text) { return new Uri("file", decodeURIComponent(text.replace(/^file:\/\//, ""))); }
  toString() { return "file://" + this.fsPath.split("/").map(encodeURIComponent).join("/"); }
}
class MarkdownString { constructor(value) { this.value = value; } }
class SnippetString { constructor(value) { this.value = value; } }
class CompletionItem { constructor(label, kind) { this.label = label; this.kind = kind; } }
class CompletionList { constructor(items, isIncomplete) { this.items = items; this.isIncomplete = isIncomplete; } }
class Hover { constructor(contents, range) { this.contents = [contents]; this.range = range; } }
class Location { constructor(uri, range) { this.uri = uri; this.range = range; } }
class Diagnostic { constructor(range, message, severity) { this.range = range; this.message = message; this.severity = severity; } }
class SignatureHelp {}
class SignatureInformation { constructor(label, doc) { this.label = label; this.documentation = doc; } }
class ParameterInformation { constructor(label) { this.label = label; } }
class DocumentSymbol { constructor(name, detail, kind, range, sel) { Object.assign(this, { name, detail, kind, range, selectionRange: sel, children: [] }); } }
class FoldingRange { constructor(s, e) { this.start = s; this.end = e; } }
class TextEdit { constructor(range, newText) { this.range = range; this.newText = newText; } }
class DocumentHighlight { constructor(range, kind) { this.range = range; this.kind = kind; } }
class WorkspaceEdit { constructor() { this.edits = []; } replace(uri, range, text) { this.edits.push({ uri, range, text }); } }
class CodeAction { constructor(title, kind) { this.title = title; this.kind = kind; } }
class SemanticTokens { constructor(data) { this.data = data; } }
class SemanticTokensLegend { constructor(t, m) { this.tokenTypes = t; this.tokenModifiers = m; } }
class Color { constructor(red, green, blue, alpha) { Object.assign(this, { red, green, blue, alpha }); } }
class ColorInformation { constructor(range, color) { this.range = range; this.color = color; } }
class ColorPresentation { constructor(label, detail) { this.label = label; this.detail = detail; } }
class EventEmitter { constructor() { this.listeners = []; this.event = (fn) => { this.listeners.push(fn); return { dispose() {} }; }; } fire(v) { for (const l of this.listeners) l(v); } }

const providers = {};
const events = { open: new EventEmitter(), change: new EventEmitter(), save: new EventEmitter(), close: new EventEmitter() };
const diagnostics = new Map();
const logs = [];

class TextDocument {
  constructor(file, text) { this.uri = Uri.file(file); this.languageId = "clpp"; this.version = 1; this.text = text; }
  getText() { return this.text; }
  lineAt(line) { return { text: this.text.split("\n")[line] || "" }; }
  getWordRangeAtPosition(position) {
    const line = this.text.split("\n")[position.line] || "";
    let s = position.character, e = position.character;
    const word = /[A-Za-z0-9_À-ɏ]/;
    while (s > 0 && word.test(line[s - 1])) s--;
    while (e < line.length && word.test(line[e])) e++;
    return s === e ? undefined : new Range(position.line, s, position.line, e);
  }
  async save() { return true; }
}

const documents = [];
const register = (kind) => (selector, provider, ...rest) => { providers[kind] = provider; return { dispose() {} }; };

module.exports = {
  Position, Range, Uri, MarkdownString, SnippetString, CompletionItem, CompletionList, Hover, Location, Diagnostic,
  SignatureHelp, SignatureInformation, ParameterInformation, DocumentSymbol, FoldingRange, TextEdit, DocumentHighlight,
  WorkspaceEdit, CodeAction, SemanticTokens, SemanticTokensLegend, EventEmitter, Color, ColorInformation, ColorPresentation,
  DiagnosticSeverity: { Error: 0, Warning: 1, Information: 2, Hint: 3 },
  CodeActionKind: { QuickFix: "quickfix" },
  StatusBarAlignment: { Left: 1 },
  version: "1.95.0-mock",
  window: {
    createOutputChannel: () => ({ appendLine: (l) => logs.push(l), show() {}, dispose() {} }),
    createStatusBarItem: () => ({ show() {}, dispose() {} }),
    showErrorMessage: async (m) => { logs.push("ERROR " + m); return undefined; },
    terminals: [], createTerminal: () => ({ show() {}, sendText() {} }), activeTextEditor: undefined,
  },
  commands: { registerCommand: () => ({ dispose() {} }), executeCommand: async () => {} },
  workspace: {
    workspaceFolders: [],
    textDocuments: documents,
    getConfiguration: () => ({ get: (key) => process.env.CLPP_SERVER_PATH && key === "serverPath" ? process.env.CLPP_SERVER_PATH : "" }),
    onDidOpenTextDocument: events.open.event, onDidChangeTextDocument: events.change.event,
    onDidSaveTextDocument: events.save.event, onDidCloseTextDocument: events.close.event,
    onDidChangeConfiguration: () => ({ dispose() {} }),
    createFileSystemWatcher: () => ({ onDidCreate: () => ({}), onDidChange: () => ({}), onDidDelete: () => ({}), dispose() {} }),
  },
  languages: {
    createDiagnosticCollection: () => ({ set: (uri, list) => diagnostics.set(uri.fsPath, list), clear() {}, dispose() {} }),
    registerCompletionItemProvider: register("completion"), registerHoverProvider: register("hover"),
    registerSignatureHelpProvider: register("signature"), registerDefinitionProvider: register("definition"),
    registerReferenceProvider: register("references"), registerDocumentHighlightProvider: register("highlight"),
    registerRenameProvider: register("rename"), registerDocumentSymbolProvider: register("symbols"),
    registerFoldingRangeProvider: register("folding"), registerDocumentFormattingEditProvider: register("format"),
    registerCodeActionsProvider: register("codeAction"), registerDocumentSemanticTokensProvider: register("semantic"),
    registerColorProvider: register("color"),
  },
  // test helpers
  __test: {
    providers, diagnostics, logs, TextDocument,
    open(doc) { documents.push(doc); events.open.fire(doc); },
    change(doc, text) { doc.text = text; doc.version++; events.change.fire({ document: doc, contentChanges: [{ text }] }); },
  },
};
