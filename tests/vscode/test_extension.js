// Activates tools/vscode/extension.js against a mocked `vscode` API and the real clpp server,
// then calls each registered provider the way VS Code does. Usage: node test_extension.js <clpp>
"use strict";
const Module = require("module");
const path = require("path");
const fs = require("fs");
const os = require("os");

const mock = require("./mock-vscode.js");
const original = Module._resolveFilename;
Module._resolveFilename = function (request, ...rest) {
  if (request === "vscode") return require.resolve("./mock-vscode.js");
  return original.call(this, request, ...rest);
};

process.env.CLPP_SERVER_PATH = path.resolve(process.argv[2]);
const extension = require("../../tools/vscode/extension.js");
const { providers, diagnostics, TextDocument } = mock.__test;
let failures = 0;
const check = (name, ok, detail) => { console.log((ok ? "PASS " : "FAIL ") + name + (ok ? "" : "  -> " + JSON.stringify(detail))); if (!ok) failures++; };
const token = { onCancellationRequested() {} };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "clpp-ext-"));
  fs.writeFileSync(path.join(dir, "shapes.clp"),
    "export def Animal {\n  number legs;\n  number speak() { return @this.legs; }\n}\nexport def Dog : Animal {\n  number bones;\n}\nexport number area(number w, number h) { return w * h; }\n");
  fs.writeFileSync(path.join(dir, 'AlertModule.clp'), 'export string Ping() { return "ok"; }\n');
  const context = { subscriptions: [], asAbsolutePath: (p) => path.join(__dirname, "../../tools/vscode", p) };
  await extension.activate(context);

  const file = path.join(dir, "main.clp");
  const doc = new TextDocument(file, 'link "./shapes.clp" as Shapes;\nShapes.Dog d = Shapes.Dog { legs = 4, bones = 2 };\npost(d.)\n');
  mock.__test.open(doc);
  await sleep(300);

  const P = (l, c) => new mock.Position(l, c);
  let list = await providers.completion.provideCompletionItems(doc, P(2, 7), token);
  const labels = list.items.map((i) => i.label);
  check("completion: inherited members through the extension", ["bones", "legs", "speak"].every((l) => labels.includes(l)), labels);
  const speak = list.items.find((i) => i.label === "speak");
  check("completion items carry documentation", speak && speak.documentation && speak.documentation.value.includes("speak"), speak);

  const initializer = 'link "./shapes.clp" as Shapes;\nauto dog = Shapes.Dog { legs = 4, bo };\n';
  const fieldStart = initializer.split('\n')[1].indexOf('bo }');
  mock.__test.change(doc, initializer);
  list = await providers.completion.provideCompletionItems(doc, P(1, fieldStart + 2), token);
  const bones = list.items.find(item => item.label === 'bones');
  check("initializer completion replaces only the field prefix", bones && bones.detail === 'number' &&
    bones.insertText === 'bones' && bones.range.start.character === fieldStart && bones.range.end.character === fieldStart + 2, bones);

  mock.__test.change(doc, 'link "./shapes.clp" as Shapes;\npost(Shapes.area(1, 2));\nconst auto x = 1;\nx = 2;\n');
  await sleep(300);
  const hover = await providers.hover.provideHover(doc, P(1, 14), token);
  check("hover returns markdown with the signature", hover && hover.contents[0].value.includes("number area(number w, number h)"), hover);
  const definition = await providers.definition.provideDefinition(doc, P(1, 14), token);
  check("definition opens shapes.clp", definition && definition[0].uri.fsPath.endsWith("shapes.clp"), definition);
  const diags = diagnostics.get(file) || [];
  check("inline error is published (error lens)", diags.some((d) => d.message === "cannot assign to immutable binding"), diags);
  const help = await providers.signature.provideSignatureHelp(doc, P(1, 17), token);
  check("signature help", help && help.signatures[0].label.startsWith("number area("), help);
  const symbols = await providers.symbols.provideDocumentSymbols(doc, token);
  check("outline", symbols.some((s) => s.name === "x"), symbols.map((s) => s.name));
  const actions = await providers.codeAction.provideCodeActions(doc, diags[0] && diags[0].range, { diagnostics: diags }, token);
  check("quick fix removes const", actions.some((a) => a.title.includes("mut")), actions.map((a) => a.title));
  const tokens = await providers.semantic.provideDocumentSemanticTokens(doc, token);
  check("semantic tokens", tokens.data.length > 0 && tokens.data.length % 5 === 0, tokens.data.length);

  mock.__test.change(doc, "link @clpp.\n");
  await sleep(100);
  list = await providers.completion.provideCompletionItems(doc, P(0, 11), token);
  check("link @clpp. modules", list.items.some((i) => i.label === "axiom"), list.items.map((i) => i.label));
  mock.__test.change(doc, "fo");
  await sleep(100);
  list = await providers.completion.provideCompletionItems(doc, P(0, 2), token);
  const snippet = list.items.find((i) => i.label === "for" && i.insertText instanceof mock.SnippetString);
  check("snippets become SnippetString", Boolean(snippet), list.items.map((i) => i.label));

  mock.__test.change(doc, 'const auto PANEL = 0x141025;\nconst auto HP = 0xFF5A8A;\n<< 0xFFFFFF\npost("0x123456");\n');
  await sleep(100);
  const colors = await providers.color.provideDocumentColors(doc, token);
  check("shared hex previews exclude comments and strings", colors.length === 2 &&
    Math.abs(colors[0].color.red - 20 / 255) < 1e-9, colors);
  const presentations = providers.color.provideColorPresentations(new mock.Color(1, 0, 0, 0.5));
  check("color presentations use valid CL++ forms", presentations.some(p => p.label.startsWith("Gfx.Rgba(")) &&
    presentations.every(p => !p.label.startsWith("#")), presentations);

  const formatInput = 'number first=1;\nnumber second=2;\nif(first < second){post("ação 😀");}';
  mock.__test.change(doc, formatInput);
  await sleep(100);
  const edits = await providers.format.provideDocumentFormattingEdits(doc, { tabSize: 4, insertSpaces: true }, token);
  check("format: editor indentation and paragraph separation", edits.length === 1 &&
    edits[0].newText.includes('number first = 1;\nnumber second = 2;\n\nif (first < second) {\n    post("ação 😀");\n}'), edits);
  check("format: replacement ends at the actual UTF-16 document end", edits.length === 1 &&
    edits[0].range.end.line === 2 && edits[0].range.end.character === formatInput.split('\n')[2].length, edits);
  const tabEdits = await providers.format.provideDocumentFormattingEdits(doc, { tabSize: 4, insertSpaces: false }, token);
  check("format: editor tabs are honored", tabEdits.length === 1 && tabEdits[0].newText.includes('\n\tpost('), tabEdits);

  mock.__test.change(doc, 'post(AlertM.Ping());');
  await sleep(100);
  const imports = await providers.completion.provideCompletionItems(doc, P(0, 11), token);
  const autoImport = imports.items.find(item => item.label === 'AlertModule' && item.detail.startsWith('Auto-import:'));
  check("auto import: VS Code maps the main and additional edits", autoImport &&
    autoImport.insertText === 'AlertModule' && autoImport.range && autoImport.additionalTextEdits &&
    autoImport.additionalTextEdits[0].newText.includes('link "./AlertModule.clp" as AlertModule;'), autoImport);
  mock.__test.change(doc, 'AlertM.Ping();');
  await sleep(100);
  const firstImports = await providers.completion.provideCompletionItems(doc, P(0, 6), token);
  const firstImport = firstImports.items.find(item => item.label === 'AlertModule' && item.detail.startsWith('Auto-import:'));
  check("auto import: first-word acceptance has no overlapping edits", firstImport &&
    firstImport.insertText.includes('link "./AlertModule.clp" as AlertModule;\n\nAlertModule') && !firstImport.additionalTextEdits, firstImport);

  await extension.deactivate();
  console.log(`${failures} failure(s)`);
  process.exit(failures);
})().catch((e) => { console.error(e); process.exit(99); });
