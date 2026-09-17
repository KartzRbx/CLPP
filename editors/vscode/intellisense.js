"use strict";

const KEYWORDS = new Set([
  "void",
  "int",
  "float",
  "double",
  "bool",
  "string",
  "auto",
  "func",
  "if",
  "else",
  "guard",
  "for",
  "while",
  "return",
  "match",
  "spawn",
  "parallel",
  "async",
  "await",
  "new",
  "null",
  "true",
  "false",
  "this",
  "observable",
  "signal",
  "array",
  "dictionary",
]);

const PRIMITIVES = new Set([
  "int",
  "float",
  "double",
  "bool",
  "string",
  "void",
  "func",
  "auto",
  "null",
  "true",
  "false",
]);

const INSTANCE_FALLBACK = new Set([
  "Instance",
  "Folder",
  "Part",
  "MeshPart",
  "BasePart",
  "Model",
  "WorldModel",
  "Actor",
  "Player",
  "Players",
  "Humanoid",
  "Workspace",
  "DataModel",
  "RemoteEvent",
  "RemoteFunction",
  "BindableEvent",
  "BindableFunction",
  "ScreenGui",
  "Frame",
  "TextLabel",
  "TextButton",
  "TextBox",
  "ImageLabel",
  "ImageButton",
  "ScrollingFrame",
  "Tool",
  "Camera",
  "Sound",
  "LocalScript",
  "ModuleScript",
  "Script",
  "ReplicatedStorage",
  "ServerStorage",
  "Lighting",
  "RunService",
]);

const NAME_TYPES = [
  [/^players$/i, "Players"],
  [/player/i, "Player"],
  [/humanoid/i, "Humanoid"],
  [/janitor/i, "Janitor"],
  [/^task$/i, "task"],
  [/character/i, "Model"],
  [/workspace/i, "Workspace"],
];

const SIGNAL_TYPES = new Set(["signal", "Signal", "RBXScriptSignal"]);

function loadEngine(data) {
  const catalog = data.catalog || {};
  const globals = data.globals || {};
  const memberCache = new Map();

  function normalizeType(raw) {
    if (!raw) {
      return null;
    }
    let t = String(raw)
      .trim()
      .replace(/^const\s+/, "")
      .replace(/\*+$/, "")
      .trim();
    if (!t) {
      return null;
    }
    if (t === "signal" || t.startsWith("signal<") || t.startsWith("Signal<") || t === "Signal") {
      return "signal";
    }
    if (t.startsWith("observable")) {
      return "observable";
    }
    if (
      t.startsWith("array<") ||
      t.startsWith("vector<") ||
      t.startsWith("span<") ||
      t === "array"
    ) {
      return "array";
    }
    if (t.startsWith("dictionary<") || t.startsWith("map<") || t === "dictionary") {
      return "dictionary";
    }
    return t;
  }

  function isSignalLike(type) {
    const t = normalizeType(type);
    return SIGNAL_TYPES.has(t);
  }

  function typeExists(name) {
    return Boolean(catalog[name]);
  }

  function collectType(typeName, seen) {
    const key = typeName || "";
    if (memberCache.has(key)) {
      return memberCache.get(key);
    }
    const out = {
      properties: [],
      methods: [],
      events: [],
      tableKeys: [],
      byName: new Map(),
    };
    const visit = (name, visiting) => {
      if (!name || visiting.has(name)) {
        return;
      }
      visiting.add(name);
      const node = catalog[name];
      if (!node) {
        return;
      }
      if (node.extends) {
        visit(node.extends, visiting);
      }
      for (const kind of ["properties", "methods", "events", "tableKeys"]) {
        for (const member of node[kind] || []) {
          const copy = { ...member, kind };
          out[kind].push(copy);
          out.byName.set(member.label, copy);
        }
      }
    };
    visit(typeName, seen || new Set());
    memberCache.set(key, out);
    return out;
  }

  function findMember(typeName, label) {
    return collectType(typeName).byName.get(label) || null;
  }

  function inferFromName(name) {
    if (!name) {
      return null;
    }
    for (const [re, type] of NAME_TYPES) {
      if (re.test(name)) {
        return type;
      }
    }
    return null;
  }

  function addVar(vars, name, type, detail) {
    if (!name || PRIMITIVES.has(name) || KEYWORDS.has(name)) {
      return;
    }
    const normalized = normalizeType(type) || inferFromName(name);
    vars.set(name, {
      type: normalized,
      detail: detail || (normalized ? `${normalized} (local)` : "local"),
    });
  }

  function parseParams(list, vars) {
    if (!list) {
      return;
    }
    for (const part of list.split(",")) {
      const trimmed = part.trim();
      if (!trimmed) {
        continue;
      }
      const m = trimmed.match(/^(.+?)\s+([A-Za-z_]\w*)$/);
      if (!m) {
        continue;
      }
      const typeTok = m[1].trim();
      addVar(vars, m[2], typeTok.startsWith("observable") ? "observable" : typeTok);
    }
  }

  function stripLiterals(src) {
    return src
      .replace(/\/\*[\s\S]*?\*\//g, " ")
      .replace(/\/\/.*$/gm, " ")
      .replace(/"(?:\\.|[^"\\])*"/g, '""')
      .replace(/'(?:\\.|[^'\\])*'/g, "''");
  }

  function balancedBody(src, openIndex) {
    let depth = 0;
    for (let i = openIndex; i < src.length; i++) {
      const ch = src[i];
      if (ch === "{") {
        depth += 1;
      } else if (ch === "}") {
        depth -= 1;
        if (depth === 0) {
          return src.slice(openIndex + 1, i);
        }
      }
    }
    return "";
  }

  function indexDocument(raw) {
    const vars = new Map();
    const dictKeys = new Map();

    const dictRe = /\bdictionary(?:<[^>]+>)?\s+([A-Za-z_]\w*)\s*=\s*\{/g;
    let match;
    while ((match = dictRe.exec(raw))) {
      const body = balancedBody(raw, match.index + match[0].length - 1);
      const keys = [];
      const keyRe = /\{\s*"([^"]+)"/g;
      let keyMatch;
      while ((keyMatch = keyRe.exec(body))) {
        keys.push(keyMatch[1]);
      }
      if (keys.length) {
        dictKeys.set(match[1], keys);
      }
      addVar(vars, match[1], "dictionary");
    }

    const text = stripLiterals(raw);

    const decl =
      /\b(?:(?:async\s+)?(?:observable\s+(\w+)|signal(?:<[^>;{]+>)?|(?:array|dictionary|map|vector|span)<[^>;{]+>|(?:const\s+)?(?:int|float|double|bool|string|auto|func)|[A-Z][A-Za-z0-9_]*(?:\s*\*)?)\s+)([A-Za-z_]\w*)\b(?!\s*\()(?=\s*[=;,:){])/g;
    while ((match = decl.exec(text))) {
      const full = match[0];
      const name = match[2];
      let typeTok = full.replace(new RegExp(`\\s+${name}\\s*$`), "").trim();
      if (typeTok.startsWith("async")) {
        typeTok = typeTok.replace(/^async\s+/, "");
      }
      if (typeTok.startsWith("observable")) {
        addVar(vars, name, "observable");
      } else {
        addVar(vars, name, typeTok);
      }
    }

    const paramLists = /\(([^;{}=]*)\)\s*(?:\{|->)/g;
    while ((match = paramLists.exec(text))) {
      parseParams(match[1], vars);
    }

    const lambdas = /func\s*\[\]\s*\(([^)]*)\)/g;
    while ((match = lambdas.exec(text))) {
      parseParams(match[1], vars);
    }

    const getService = /\b([A-Za-z_]\w*)\s*=\s*(?:[\w:]+::)?GetService\s*<\s*([A-Za-z_]\w*)\s*>/g;
    while ((match = getService.exec(text))) {
      addVar(vars, match[1], match[2], `${match[2]} (GetService)`);
    }

    for (const [name, info] of vars) {
      if (!info.type || info.type === "auto") {
        const inferred = inferFromName(name);
        if (inferred) {
          info.type = inferred;
          info.detail = `${inferred} (inferred)`;
        }
      }
    }

    return { vars, dictKeys };
  }

  function parseAccess(line) {
    const ident = "[A-Za-z_][A-Za-z0-9_]*";
    const acc = "(?:~>|::|\\.:|:|\\.)";
    const generic = "(?:<[^;<>]*>)?";
    const call = "(?:\\([^;]*\\))?";
    const re = new RegExp(
      `(${ident})(${generic})(${call})((?:${acc}${ident}${generic}${call})*)(${acc})(${ident})?$`
    );
    const m = line.match(re);
    if (!m) {
      return null;
    }
    const accessor = m[5];
    if (accessor === ".:") {
      return { mode: "concat" };
    }
    const steps = [];
    const stepRe = /(~>|::|:|\.)([A-Za-z_][A-Za-z0-9_]*)(<[^;<>]*>)?(\([^;]*\))?/g;
    let step;
    while ((step = stepRe.exec(m[4] || ""))) {
      steps.push({
        accessor: step[1],
        name: step[2],
        generic: step[3] ? step[3].slice(1, -1).trim() : null,
        call: Boolean(step[4]),
      });
    }
    const mode =
      accessor === "~>"
        ? "cleanup"
        : accessor === "::"
          ? "method"
          : accessor === "."
            ? "property"
            : "table";
    return {
      mode,
      root: m[1],
      generic: m[2] ? m[2].slice(1, -1).trim() : null,
      call: Boolean(m[3]),
      steps,
      accessor,
      partial: m[6] || "",
    };
  }

  function typeOfRoot(root, generic, call, symbols) {
    if (root === "GetService" && generic) {
      return generic;
    }
    if (symbols.vars.has(root)) {
      return symbols.vars.get(root).type;
    }
    if (globals[root]) {
      return globals[root].type;
    }
    if (typeExists(root) && call) {
      return root;
    }
    return inferFromName(root);
  }

  function walkType(startType, steps) {
    let current = startType;
    for (const step of steps) {
      if (!current) {
        return null;
      }
      if (step.name === "GetService" && step.generic) {
        current = step.generic;
        continue;
      }
      const member = findMember(current, step.name);
      if (!member) {
        return null;
      }
      current = normalizeType(member.returns || member.type);
    }
    return current;
  }

  function fallbackType(type) {
    if (!type || PRIMITIVES.has(type)) {
      return type;
    }
    if (catalog[type]) {
      return type;
    }
    if (INSTANCE_FALLBACK.has(type) || /^[A-Z]/.test(type)) {
      return "Instance";
    }
    return type;
  }

  function membersFor(type, mode, symbols, root) {
    if (mode === "concat") {
      return [];
    }
    const dictKeys =
      root && symbols.dictKeys.has(root)
        ? symbols.dictKeys.get(root).map((label) => ({
            label,
            kind: "tableKeys",
            type: "auto",
            detail: `${root}:${label}`,
          }))
        : [];

    if (mode === "table" && dictKeys.length && (type === "dictionary" || !type)) {
      return dictKeys;
    }

    const resolved = fallbackType(type);
    if (!resolved || PRIMITIVES.has(resolved)) {
      if (mode === "table" && dictKeys.length) {
        return dictKeys;
      }
      return [];
    }

    const bag = collectType(resolved);

    if (mode === "property") {
      const props = [...bag.properties];
      if (type === "dictionary" || dictKeys.length) {
        return props.concat(dictKeys);
      }
      return props;
    }

    if (mode === "method") {
      return [...bag.methods, ...bag.events];
    }

    if (mode === "cleanup") {
      if (!isSignalLike(resolved) && resolved !== "signal") {
        return [];
      }
      return (bag.methods || []).filter(
        (m) => m.label === "Connect" || m.label === "Once"
      );
    }

    if (mode === "table") {
      if (bag.tableKeys.length) {
        return bag.tableKeys;
      }
      if (isSignalLike(resolved)) {
        return bag.methods;
      }
      if (dictKeys.length) {
        return dictKeys;
      }
      return [];
    }

    return [];
  }

  function resolve(line, symbols) {
    const access = parseAccess(line);
    if (!access || access.mode === "concat") {
      return { mode: access ? "concat" : "global", members: [], access };
    }
    const rootType = typeOfRoot(access.root, access.generic, access.call, symbols);
    const type = walkType(rootType, access.steps);
    const members = membersFor(type, access.mode, symbols, access.root);
    return { mode: access.mode, type, members, access, rootType };
  }

  function hoverFor(word, symbols) {
    if (symbols.vars.has(word)) {
      const info = symbols.vars.get(word);
      return info.detail || info.type || "local";
    }
    if (globals[word]) {
      return globals[word].detail || globals[word].type;
    }
    for (const typeName of Object.keys(catalog)) {
      const member = findMember(typeName, word);
      if (member && catalog[typeName].methods?.some((m) => m.label === word) && typeName !== "Instance") {
        if (member.detail) {
          return member.detail;
        }
      }
    }
    const owned = findMember("signal", word) || findMember("RBXScriptSignal", word);
    if (owned?.detail) {
      return owned.detail;
    }
    return null;
  }

  return {
    indexDocument,
    parseAccess,
    resolve,
    membersFor,
    hoverFor,
    normalizeType,
    typeOfRoot,
  };
}

module.exports = { loadEngine, PRIMITIVES, lintDocument };

function lintDocument(text) {
  const diagnostics = [];
  const lines = text.split(/\r?\n/);
  const skip = /^(if|else|for|while|guard|match|switch|spawn|parallel|struct|class|namespace)\b/;
  for (let i = 0; i < lines.length; i++) {
    const raw = lines[i];
    const trimmed = raw.trim();
    if (!trimmed || trimmed.startsWith("//") || trimmed.startsWith("#") || trimmed.startsWith("[[")) {
      continue;
    }
    if (/\b(?:const|constexpr)\s+(int|float|double|bool|string)\s*=/.test(trimmed)) {
      diagnostics.push({
        line: i,
        column: Math.max(0, raw.indexOf("=")),
        message: "expected a name after the type, e.g. const int coins = 0;",
      });
    }
    const assign = trimmed.match(
      /\b((?:const|constexpr)\s+)?(int|float|double|bool)\s+([A-Za-z_]\w*)\s*=\s*(.+)$/
    );
    if (assign) {
      const ty = assign[2];
      const name = assign[3];
      const rhs = assign[4].replace(/;+\s*$/, "").trim();
      const isText = /^['"`]/.test(rhs) || rhs.startsWith("`");
      if (isText && (ty === "int" || ty === "float" || ty === "double")) {
        const qual = assign[1] ? "const " : "";
        diagnostics.push({
          line: i,
          column: Math.max(0, raw.indexOf("=")),
          message: `cannot initialize '${qual}${ty} ${name}' with string; expected ${ty}`,
        });
      }
    }
    const needsSemi =
      /^(?:const|constexpr|observable|auto|int|float|double|bool|string|func|return|break|post|warn|report|[A-Z][A-Za-z0-9_]*\*?)\b/.test(
        trimmed
      ) || /^[A-Za-z_]\w*\s*[+\-*/]?=/.test(trimmed);
    if (
      needsSemi &&
      !skip.test(trimmed) &&
      !trimmed.endsWith(";") &&
      !trimmed.endsWith("{") &&
      !trimmed.endsWith(",") &&
      !trimmed.endsWith(")") &&
      !trimmed.includes("=>") &&
      !trimmed.endsWith("\\")
    ) {
      diagnostics.push({
        line: i,
        column: Math.max(0, raw.length - 1),
        message: "missing ';' at the end of this statement",
      });
    }
  }
  return diagnostics;
}
