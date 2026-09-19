"use strict";

const { atCompletions, enclosingOwner } = require("./intellisense");

const HOVER_WORDS = {
  post: "Write a line to output.",
  warn: "Write a warning.",
  report: "Stop the script with an error.",
  observable:
    "Reactive value. Assign to update (`coins = 50`); listen with `.OnChange`.",
  signal:
    "`signal<T...>` — `.Fire(...)` sends, `~>Connect` / `~>Once` listen (Once runs once).",
  Fire: "`signal.Fire(...)` — send the signal.",
  Connect: "Subscribe until Disconnect. Prefer `~>Connect` for automatic cleanup.",
  Once: "Subscribe for a single emission, then disconnect. `~>Once` is cleaned up for you.",
  Wait: "Pause until the next emission.",
  OnChange: "Run a function whenever an observable changes.",
  GetPropertyChangedSignal:
    '`instance.GetPropertyChangedSignal("Name")` — signal for one property.',
  guard: "Continue only when the condition is true; otherwise run the else block.",
  in: "Range-for: each value comes from the collection after `in`.",
  await: "Wait until an async value is ready.",
  to_string: "Convert a value to text. Emits Luau tostring.",
  to_number: "Parse a number from text. Emits Luau tonumber. Fails → null.",
  this: "Receiver alias inside Class::Method. Emits self. Prefer @this.",
  "@this":
    "`@` is the receiver sigil. `@this` is the current object (Luau self). Only inside Class::Method.",
};

function symbolsFor(engine, text, extraTexts) {
  return engine.indexDocument(text, extraTexts || []);
}

function buildCompletionItems(engine, data, text, filePath, lineText, offset, folders, extraTexts) {
  if (/GetService\s*<\s*[A-Za-z_]*$/.test(lineText)) {
    return (data.services || data.types || []).map((ty) => ({
      label: ty,
      kind: "Class",
      detail: `GetService<${ty}>()`,
    }));
  }
  const symbols = symbolsFor(engine, text, extraTexts);
  const at = atCompletions(lineText, symbols, enclosingOwner(text, offset));
  if (at) {
    return at;
  }
  const resolved = engine.resolve(lineText, symbols);
  if (resolved.mode !== "global" && resolved.mode !== "concat") {
    return resolved.members.map((member) => ({
      label: member.label,
      kind: member.kind,
      detail: member.detail,
    }));
  }
  const items = [];
  for (const word of data.keywords || []) {
    items.push({ label: word, kind: "Keyword", detail: "CL++ keyword" });
  }
  for (const fn of data.builtins || []) {
    items.push({
      label: fn.label,
      kind: fn.kind || "Function",
      detail: fn.detail,
    });
  }
  for (const ty of data.types || []) {
    items.push({ label: ty, kind: "Class", detail: "CL++ / Roblox type" });
  }
  for (const op of data.operators || []) {
    items.push({ label: op.label, kind: "Operator", detail: op.detail });
  }
  for (const [name, info] of symbols.vars) {
    items.push({ label: name, kind: "Variable", detail: info.detail });
  }
  return items;
}

function hoverText(engine, text, filePath, word, folders, extraTexts) {
  if (!word) {
    return null;
  }
  if (HOVER_WORDS[word]) {
    return HOVER_WORDS[word];
  }
  return engine.hoverFor(word, symbolsFor(engine, text, extraTexts));
}

module.exports = { HOVER_WORDS, symbolsFor, buildCompletionItems, hoverText };
