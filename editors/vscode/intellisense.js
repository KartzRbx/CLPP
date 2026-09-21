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
  "in",
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
  "@this",
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

  function typeNode(name, overlay) {
    if (overlay && overlay[name]) {
      return overlay[name];
    }
    return catalog[name] || null;
  }

  function typeExists(name, overlay) {
    return Boolean(typeNode(name, overlay));
  }

  function pickTemplate(overlay) {
    if (!overlay) {
      return null;
    }
    if (overlay.PlayerData) {
      return "PlayerData";
    }
    const names = Object.keys(overlay).filter((n) => /Data$/.test(n) && n !== "Data");
    return names[0] || null;
  }

  function withPathTree(overlay) {
    const template = pickTemplate(overlay);
    if (!template) {
      return overlay || {};
    }
    const extra = { ...overlay };
    extra.DataService = {
      tableKeys: (catalog.DataService?.tableKeys || []).map((key) =>
        key.label === "Paths"
          ? { ...key, type: template, detail: `DataService:Paths (${template})` }
          : key
      ),
    };
    const pathField = {
      label: "Paths",
      type: template,
      detail: `Paths (${template})`,
    };
    extra.DataServiceServer = mergePathOwner(catalog.DataServiceServer, pathField);
    extra.DataServiceClient = mergePathOwner(catalog.DataServiceClient, pathField);
    return extra;
  }

  function mergePathOwner(node, pathField) {
    const withoutPaths = (list) => (list || []).filter((member) => member.label !== "Paths");
    return {
      ...(node || {}),
      properties: [...withoutPaths(node?.properties), pathField],
      tableKeys: [...withoutPaths(node?.tableKeys), pathField],
      methods: [...(node?.methods || [])],
    };
  }

  function collectType(typeName, overlay, seen) {
    const extra = overlay || {};
    const key = `${typeName || ""}::${Object.keys(extra).sort().join(",")}`;
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
      const node = typeNode(name, extra);
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

  function findMember(typeName, label, overlay) {
    return collectType(typeName, overlay).byName.get(label) || null;
  }

  function stripInnerBlocks(src) {
    let out = src;
    let next = out.replace(/\{[^{}]*\}/g, " ");
    while (next !== out) {
      out = next;
      next = out.replace(/\{[^{}]*\}/g, " ");
    }
    return out;
  }

  function parseStructs(src) {
    const types = {};
    const re = /\b(?:struct|namespace|class)\s+([A-Za-z_]\w*)\s*(?::[^{]*)?\{/g;
    let match;
    while ((match = re.exec(src))) {
      const name = match[1];
      const body = stripInnerBlocks(balancedBody(src, match.index + match[0].length - 1));
      const node = { properties: [], methods: [] };
      for (const chunk of body.split(";")) {
        const line = chunk.replace(/\b(public|private|protected)\s*:/g, " ").trim();
        if (!line || line.startsWith("#")) {
          continue;
        }
        const method = line.match(
          /^(?:(?:static|virtual|inline|constexpr|const)\s+)*(.+?)\s+([A-Za-z_]\w*)\s*\(/
        );
        if (method) {
          const ret = method[1].replace(/\b(?:static|virtual|inline|constexpr|const)\b/g, "").trim();
          if (ret && ret !== "struct" && ret !== "namespace" && ret !== "class") {
            node.methods.push({
              label: method[2],
              returns: ret,
              detail: `${name}::${method[2]}()`,
              kind: "methods",
            });
          }
          continue;
        }
        const field = line.match(
          /^(?:(?:static|constexpr|const|mutable)\s+)*(.+?)\s+([A-Za-z_]\w*)\s*(?:=\s*.*)?$/
        );
        if (!field) {
          continue;
        }
        const ty = field[1].replace(/\b(?:static|constexpr|const|mutable)\b/g, "").trim();
        if (!ty || ty === "struct" || ty === "namespace" || ty === "class") {
          continue;
        }
        node.properties.push({
          label: field[2],
          type: ty.replace(/\*+$/, "").trim(),
          detail: `${name}.${field[2]}`,
          kind: "properties",
        });
      }
      types[name] = node;
    }
    return types;
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

  function indexDocument(raw, extraTexts) {
    const vars = new Map();
    const dictKeys = new Map();
    const types = parseStructs(stripLiterals(raw));
    for (const extra of extraTexts || []) {
      Object.assign(types, parseStructs(stripLiterals(extra)));
    }
    const implRe =
      /\b(?:(?:const|static|constexpr|async)\s+)*([A-Za-z_]\w*(?:<[^>]+>)?)\s+([A-Z][A-Za-z0-9_]*)::([A-Za-z_]\w*)\s*\(/g;
    const implSrc = [raw, ...(extraTexts || [])].join("\n");
    let implMatch;
    while ((implMatch = implRe.exec(implSrc))) {
      const ret = implMatch[1];
      const ownerName = implMatch[2];
      const methodName = implMatch[3];
      if (!types[ownerName]) {
        types[ownerName] = { properties: [], methods: [] };
      }
      if (!types[ownerName].methods.some((method) => method.label === methodName)) {
        types[ownerName].methods.push({
          label: methodName,
          returns: ret,
          detail: `${ownerName}::${methodName}()`,
          kind: "methods",
        });
      }
    }
    const overlay = withPathTree(types);

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

    const lambdas = /func\s*\(([^)]*)\)\s*\{/g;
    while ((match = lambdas.exec(text))) {
      parseParams(match[1], vars);
    }

    const getService = /\b([A-Za-z_]\w*)\s*=\s*(?:[\w:]+::)?GetService\s*<\s*([A-Za-z_]\w*)\s*>/g;
    while ((match = getService.exec(text))) {
      addVar(vars, match[1], match[2], `${match[2]} (GetService)`);
    }

    const rangeFor =
      /\bfor\s*\(\s*(?:const\s+)?([A-Z][A-Za-z0-9_]*)\s+([A-Za-z_]\w*)\s+(?:in|:)/g;
    while ((match = rangeFor.exec(text))) {
      addVar(vars, match[2], match[1], `${match[1]} (for)`);
    }

    const functions = new Map();
    const fnRe =
      /\b((?:async\s+)?(?:void|int|float|double|bool|string|auto|func|[A-Z][A-Za-z0-9_]*))\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*\{/g;
    while ((match = fnRe.exec(text))) {
      const ret = match[1].replace(/^async\s+/, "").trim();
      const name = match[2];
      if (KEYWORDS.has(name) || name === "if" || name === "for" || name === "while") {
        continue;
      }
      const params = match[3].trim();
      functions.set(name, {
        returnType: ret,
        params,
        detail: `${ret} ${name}(${params})`,
      });
      addVar(vars, name, "func", `${ret} ${name}(${params})`);
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

    return { vars, dictKeys, types: overlay, templateType: pickTemplate(types), functions };
  }

  function parseAccess(line) {
    const ident = "[A-Za-z_][A-Za-z0-9_]*";
    const rootIdent = `(?:@this|@${ident}|${ident})`;
    const acc = "(?:~>|::|\\.:|:|\\.)";
    const generic = "(?:<[^;<>]*>)?";
    const call = "(?:\\([^;]*\\))?";
    const re = new RegExp(
      `(${rootIdent})(${generic})(${call})((?:${acc}${ident}${generic}${call})*)(${acc})(${ident})?$`
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

  function typeOfRoot(root, generic, call, symbols, owner) {
    if (root === "this" || root === "@this") {
      return owner || null;
    }
    if (root && root.startsWith("@") && root.length > 1) {
      const field = root.slice(1);
      if (field === "this") {
        return owner || null;
      }
      const overlay = symbols && symbols.types;
      if (owner) {
        const member = findMember(owner, field, overlay);
        if (member) {
          return normalizeType(member.returns || member.type);
        }
      }
      for (const typeName of Object.keys(overlay || {})) {
        const member = findMember(typeName, field, overlay);
        if (member && (member.kind === "properties" || member.type)) {
          return normalizeType(member.returns || member.type);
        }
      }
    }
    if (root === "GetService" && generic) {
      return generic;
    }
    if (symbols.vars.has(root)) {
      return symbols.vars.get(root).type;
    }
    if (globals[root]) {
      return globals[root].type;
    }
    const overlay = symbols && symbols.types;
    if (typeExists(root, overlay) || catalog[root]) {
      return root;
    }
    return inferFromName(root);
  }

  function walkType(startType, steps, overlay) {
    let current = startType;
    for (const step of steps) {
      if (!current) {
        return null;
      }
      if (step.name === "GetService" && step.generic) {
        current = step.generic;
        continue;
      }
      const member = findMember(current, step.name, overlay);
      if (!member) {
        return null;
      }
      current = normalizeType(member.returns || member.type);
    }
    return current;
  }

  function fallbackType(type, overlay) {
    if (!type || PRIMITIVES.has(type)) {
      return type;
    }
    if (typeExists(type, overlay)) {
      return type;
    }
    if (INSTANCE_FALLBACK.has(type)) {
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

    const overlay = symbols && symbols.types;
    const resolved = fallbackType(type, overlay);
    if (!resolved || PRIMITIVES.has(resolved)) {
      if (mode === "table" && dictKeys.length) {
        return dictKeys;
      }
      return [];
    }

    const bag = collectType(resolved, overlay);

    if (mode === "property") {
      const props = [...bag.properties, ...bag.methods, ...bag.events];
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
      if (dictKeys.length) {
        return dictKeys;
      }
      return [...bag.methods, ...bag.events];
    }

    return [];
  }

  function resolve(line, symbols, owner) {
    const access = parseAccess(line);
    if (!access || access.mode === "concat") {
      return { mode: access ? "concat" : "global", members: [], access };
    }
    const overlay = symbols && symbols.types;
    const rootType = typeOfRoot(access.root, access.generic, access.call, symbols, owner);
    const type = walkType(rootType, access.steps, overlay);
    const members = membersFor(type, access.mode, symbols, access.root);
    return { mode: access.mode, type, members, access, rootType };
  }

  function hoverFor(word, symbols) {
    if (word === "@this") {
      return "`@` marks the receiver. `@this` is the current object (Luau self). Only inside Class::Method.";
    }
    if (word === "this") {
      return "Receiver alias inside Class::Method. Emits self. Prefer @this.";
    }
    if (word && word.startsWith("@") && word.length > 1) {
      const field = word.slice(1);
      const overlay = symbols && symbols.types;
      for (const typeName of Object.keys(overlay || {})) {
        const member = findMember(typeName, field, overlay);
        if (member && member.detail) {
          return `\`@${field}\` is the receiver field (self.${field}). ${member.detail}`;
        }
      }
      return `\`@${field}\` is the receiver field — emits self.${field}. Only inside Class::Method.`;
    }
    if (symbols.vars.has(word)) {
      const info = symbols.vars.get(word);
      return info.detail || info.type || "local";
    }
    if (symbols.functions && symbols.functions.has(word)) {
      return symbols.functions.get(word).detail;
    }
    if (globals[word]) {
      return globals[word].detail || globals[word].type;
    }
    const overlay = symbols && symbols.types;
    const typeNames = new Set([...Object.keys(catalog), ...Object.keys(overlay || {})]);
    for (const typeName of typeNames) {
      const member = findMember(typeName, word, overlay);
      const node = typeNode(typeName, overlay);
      if (member && node?.methods?.some((m) => m.label === word) && typeName !== "Instance") {
        if (member.detail) {
          return member.detail;
        }
      }
      if (member && member.kind === "properties" && member.detail) {
        return member.detail;
      }
    }
    const owned = findMember("signal", word, overlay) || findMember("RBXScriptSignal", word, overlay);
    if (owned?.detail) {
      return owned.detail;
    }
    return null;
  }

  return {
    indexDocument,
    parseAccess,
    parseStructs,
    resolve,
    membersFor,
    hoverFor,
    normalizeType,
    typeOfRoot,
  };
}

function enclosingOwner(text, offset) {
  const before = text.slice(0, Math.max(0, offset));
  let last = null;
  const re = /\b([A-Za-z_]\w*)::[A-Za-z_]\w*\s*\(/g;
  let match;
  while ((match = re.exec(before))) {
    last = match[1];
  }
  return last;
}

const STATIC_AT_ITEMS = [
  {
    label: "@this",
    kind: "Keyword",
    detail: "@this — receiver sigil + current object (Luau self)",
  },
  {
    label: "@janitor",
    kind: "Keyword",
    detail: "@janitor → self.janitor (Janitor field)",
  },
];

function atCompletions(line, symbols, owner) {
  if (!/@[A-Za-z_]*$/.test(line)) {
    return null;
  }
  const items = STATIC_AT_ITEMS.map((item) => ({ ...item }));
  const seen = new Set(items.map((item) => item.label.slice(1)));
  const types = (symbols && symbols.types) || {};
  const nodes = owner && types[owner] ? [types[owner]] : Object.values(types);
  for (const node of nodes) {
    for (const member of [...(node.properties || []), ...(node.methods || [])]) {
      if (seen.has(member.label)) {
        continue;
      }
      seen.add(member.label);
      items.push({
        label: `@${member.label}`,
        kind: member.kind === "methods" ? "Method" : "Property",
        detail: member.detail || `@${member.label} → self.${member.label}`,
      });
    }
  }
  return items;
}

function lintReceiverSigil(text) {
  const diagnostics = [];
  const lines = text.split(/\r?\n/);
  let brace = 0;
  let methodDepth = 0;
  let pendingMethod = false;
  for (let i = 0; i < lines.length; i++) {
    const raw = lines[i];
    const code = raw.replace(/\/\/.*$/, "").replace(/"(?:\\.|[^"\\])*"/g, '""');
    if (/\b[A-Za-z_]\w*\s*::\s*[A-Za-z_]\w*\s*\(/.test(code)) {
      pendingMethod = !/;\s*$/.test(code.trim()) || code.includes("{");
    }
    for (let c = 0; c < code.length; c++) {
      const ch = code[c];
      if (ch === "{") {
        brace += 1;
        if (pendingMethod) {
          methodDepth = brace;
          pendingMethod = false;
        }
      } else if (ch === "}") {
        if (methodDepth && brace === methodDepth) {
          methodDepth = 0;
        }
        brace = Math.max(0, brace - 1);
      }
    }
    const atRe = /@([A-Za-z_]\w*)/g;
    let match;
    while ((match = atRe.exec(code))) {
      if (methodDepth) {
        continue;
      }
      const name = match[1];
      diagnostics.push({
        line: i,
        column: Math.max(0, raw.indexOf(match[0])),
        message:
          name === "this"
            ? "`@this` is only valid inside Class::Method"
            : `\`@${name}\` is only valid inside Class::Method`,
      });
    }
  }
  return diagnostics;
}

module.exports = {
  loadEngine,
  PRIMITIVES,
  STATIC_AT_ITEMS,
  lintDocument,
  lintReceiverSigil,
  enclosingOwner,
  atCompletions,
};

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
    if (
      /\berror\s*\(/.test(trimmed) &&
      !trimmed.startsWith("report")
    ) {
      diagnostics.push({
        line: i,
        column: Math.max(0, raw.indexOf("error")),
        message: "use report(...) — Luau error() is report() in CL++",
      });
    }
    if (/\btostring\s*\(/.test(trimmed)) {
      diagnostics.push({
        line: i,
        column: Math.max(0, raw.indexOf("tostring")),
        message: "use to_string(...) — Luau tostring() is to_string() in CL++",
      });
    }
    if (/\btonumber\s*\(/.test(trimmed)) {
      diagnostics.push({
        line: i,
        column: Math.max(0, raw.indexOf("tonumber")),
        message: "use to_number(...) — Luau tonumber() is to_number() in CL++",
      });
    }
    if (/\bcontinue\b/.test(trimmed)) {
      diagnostics.push({
        line: i,
        column: Math.max(0, raw.indexOf("continue")),
        message: "CL++ has no continue",
      });
    }
    if (/\bfunc\s*\[\]/.test(trimmed) || /^\s*\[\]\s*\(/.test(raw)) {
      diagnostics.push({
        line: i,
        column: Math.max(0, raw.search(/func\s*\[\]|\[\]\s*\(/)),
        message: "use `func (params) { }` — CL++ does not use captures `[]`",
      });
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
  for (const issue of lintReceiverSigil(text)) {
    diagnostics.push(issue);
  }
  return diagnostics;
}
