"use strict";

const { atCompletions, enclosingOwner } = require("./intellisense");
const { includePathCompletions } = require("./include-cache");

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

function designatedInitCompletions(text, offset, lineText, data, symbols) {
  const lineOk =
    /^\s*\.[A-Za-z_]*$/.test(lineText) || /Init\s*\(\s*\{\s*\.?[A-Za-z_]*$/.test(lineText);
  if (!lineOk) {
    return null;
  }
  const before = text.slice(0, offset);
  const match = before.match(/Init\s*\(\s*\{[\s\S]*$/);
  if (!match) {
    return null;
  }
  let depth = 0;
  for (const ch of match[0]) {
    if (ch === "{") {
      depth += 1;
    } else if (ch === "}") {
      depth -= 1;
    }
  }
  if (depth < 1) {
    return null;
  }
  if (depth >= 2 && symbols && symbols.templateType && symbols.types[symbols.templateType]) {
    return (symbols.types[symbols.templateType].properties || []).map((member) => ({
      label: member.label.startsWith(".") ? member.label : `.${member.label}`,
      kind: "Property",
      detail: member.detail || `${symbols.templateType} template field`,
    }));
  }
  return (data.catalog && data.catalog.DataServiceOptions
    ? data.catalog.DataServiceOptions.properties
    : []
  ).map((member) => ({
    label: member.label.startsWith(".") ? member.label : `.${member.label}`,
    kind: "Property",
    detail: member.detail || "DataService.Server.Init option",
  }));
}

function buildCompletionItems(engine, data, text, filePath, lineText, offset, folders, extraTexts) {
  if (
    /^\s*#include\s+"[^"]*$/.test(lineText) ||
    /^\s*#include\s+"[^"]*\/[^"]*$/.test(lineText) ||
    /^\s*#include\s+<[^>]*$/.test(lineText)
  ) {
    return includePathCompletions(lineText, filePath, folders);
  }
  const designated = designatedInitCompletions(text, offset, lineText, data, symbolsFor(engine, text, extraTexts));
  if (designated && designated.length) {
    return designated;
  }
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
  for (const [name, info] of symbols.functions || []) {
    items.push({
      label: name,
      kind: "Function",
      detail: info.detail || `${info.returnType} ${name}()`,
    });
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
