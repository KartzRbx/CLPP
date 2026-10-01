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
    "struct Animal {\n  int legs;\n  func speak() { return self.legs; }\n}\nstruct Dog : Animal {\n  int bones;\n}\nfunc area(int w, int h) -> int { return w * h; }\n");
  const context = { subscriptions: [], asAbsolutePath: (p) => path.join(__dirname, "../../tools/vscode", p) };
  await extension.activate(context);

  const file = path.join(dir, "main.clp");
  const doc = new TextDocument(file, 'link "./shapes.clp" as Shapes;\nDog d = Shapes.Dog(4, 2);\npost(d.)\n');
  mock.__test.open(doc);
  await sleep(300);

  const P = (l, c) => new mock.Position(l, c);
  let list = await providers.completion.provideCompletionItems(doc, P(2, 7), token);
  const labels = list.items.map((i) => i.label);
  check("completion: inherited members through the extension", ["bones", "legs", "speak"].every((l) => labels.includes(l)), labels);
  const speak = list.items.find((i) => i.label === "speak");
  check("completion items carry documentation", speak && speak.documentation && speak.documentation.value.includes("speak"), speak);

  mock.__test.change(doc, 'link "./shapes.clp" as Shapes;\npost(Shapes.area(1, 2));\nlet x = 1;\nx = 2;\n');
  await sleep(300);
  const hover = await providers.hover.provideHover(doc, P(1, 14), token);
  check("hover returns markdown with the signature", hover && hover.contents[0].value.includes("func area(int w, int h) -> int"), hover);
  const definition = await providers.definition.provideDefinition(doc, P(1, 14), token);
  check("definition opens shapes.clp", definition && definition[0].uri.fsPath.endsWith("shapes.clp"), definition);
  const diags = diagnostics.get(file) || [];
  check("inline error is published (error lens)", diags.some((d) => d.message === "cannot assign to immutable binding"), diags);
  const help = await providers.signature.provideSignatureHelp(doc, P(1, 17), token);
  check("signature help", help && help.signatures[0].label.startsWith("area("), help);
  const symbols = await providers.symbols.provideDocumentSymbols(doc, token);
  check("outline", symbols.some((s) => s.name === "x"), symbols.map((s) => s.name));
  const actions = await providers.codeAction.provideCodeActions(doc, diags[0] && diags[0].range, { diagnostics: diags }, token);
  check("quick fix let mut", actions.some((a) => a.title.includes("mut")), actions.map((a) => a.title));
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

  await extension.deactivate();
  console.log(`${failures} failure(s)`);
  process.exit(failures);
})().catch((e) => { console.error(e); process.exit(99); });
