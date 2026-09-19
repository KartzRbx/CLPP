"use strict";

const assert = require("assert");
const { createSession } = require("./lsp-server");
const { atCompletions, enclosingOwner, lintDocument } = require("./intellisense");
const { buildCompletionItems, hoverText } = require("./complete");
const { loadEngine } = require("./intellisense");
const fs = require("fs");
const path = require("path");

const data = JSON.parse(fs.readFileSync(path.join(__dirname, "data", "completions.json"), "utf8"));
const engine = loadEngine(data);

const methodSrc = `struct Service {
    Janitor janitor;
    void Tick();
};

void Service::Tick() {
    @
}
`;
assert.strictEqual(enclosingOwner(methodSrc, methodSrc.lastIndexOf("@") + 1), "Service");
const atItems = atCompletions("    @", engine.indexDocument(methodSrc), "Service");
assert.ok(atItems.some((item) => item.label === "@this"));
assert.ok(atItems.some((item) => item.label === "@janitor"));

const built = buildCompletionItems(
  engine,
  data,
  methodSrc,
  "Service.clpp",
  "    @",
  methodSrc.lastIndexOf("@") + 1,
  []
);
assert.ok(built.some((item) => item.label === "@this"));

const globalItems = buildCompletionItems(
  engine,
  data,
  "void init() {\n  \n}\n",
  "Service.clpp",
  "  ",
  15,
  []
);
assert.ok(globalItems.some((item) => item.label === "void"));
assert.ok(globalItems.some((item) => item.label === "GetService"));
assert.ok(globalItems.some((item) => item.label === "Player"));

assert.ok(hoverText(engine, methodSrc, "Service.clpp", "@this", []).includes("sigil"));
assert.ok(hoverText(engine, methodSrc, "Service.clpp", "@janitor", []).includes("self.janitor"));

const session = createSession();
const replies = [];
const send = (msg) => replies.push(msg);
const uri = "file:///tmp/Service.server.clpp";
session.handle(
  { jsonrpc: "2.0", id: 1, method: "initialize", params: { workspaceFolders: [] } },
  send
);
assert.ok(replies[0].result.capabilities.completionProvider);
assert.deepStrictEqual(replies[0].result.capabilities.completionProvider.triggerCharacters, [
  ".",
  ":",
  ">",
  "@",
]);
assert.ok(replies[0].result.capabilities.hoverProvider);
assert.ok(replies[0].result.capabilities.definitionProvider);

const emptyAt = atCompletions("    @", { types: {} }, null);
assert.ok(emptyAt.some((item) => item.label === "@this"));
assert.ok(emptyAt.some((item) => item.label === "@janitor"));

session.handle(
  {
    method: "textDocument/didOpen",
    params: {
      textDocument: {
        uri,
        languageId: "clpp",
        version: 1,
        text: `void init() {
    @this;
}
`,
      },
    },
  },
  send
);
const issues = session.flushDiagnostics(uri, send);
assert.ok(
  issues.some((d) => d.message.includes("@this") && d.message.includes("Class::Method")),
  JSON.stringify(issues)
);

session.handle(
  {
    method: "textDocument/didChange",
    params: {
      textDocument: { uri, version: 2 },
      contentChanges: [
        {
          text: `void Service::Tick() {
    @this;
}
`,
        },
      ],
    },
  },
  send
);
session.handle(
  {
    jsonrpc: "2.0",
    id: 2,
    method: "textDocument/hover",
    params: { textDocument: { uri }, position: { line: 1, character: 6 } },
  },
  send
);
const hover = replies.find((m) => m.id === 2);
assert.ok(hover.result && hover.result.contents.value.includes("self"), JSON.stringify(hover));

session.handle(
  {
    jsonrpc: "2.0",
    id: 3,
    method: "textDocument/completion",
    params: { textDocument: { uri }, position: { line: 1, character: 5 } },
  },
  send
);
const completion = replies.find((m) => m.id === 3);
assert.ok(
  (completion.result || []).some((item) => item.label === "@this"),
  JSON.stringify(completion)
);

assert.ok(lintDocument(`void init() { @this; }`).some((d) => d.message.includes("@this")));

console.log("lsp-server.test.js ok");
