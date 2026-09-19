"use strict";

const fs = require("fs");
const path = require("path");
const { resolveOnDisk } = require("./include-cache");

const SKIP = new Set([
  "node_modules",
  "target",
  ".git",
  "out",
  "build",
  "dist",
  "www",
  ".astro",
  ".moonwave",
  ".odr",
  ".tmp",
]);
const SOURCE_EXT = new Set([".clpp", ".clp", ".clh"]);

function includeSearchRoots(folders) {
  const roots = [];
  for (const folder of folders || []) {
    roots.push(folder);
    roots.push(path.join(folder, "stdlib"));
  }
  roots.push(path.join(__dirname, "..", ".."));
  roots.push(path.join(__dirname, "..", "..", "stdlib"));
  const local = process.env.LOCALAPPDATA || "";
  if (local) {
    roots.push(path.join(local, "Programs", "CLPP"));
    roots.push(path.join(local, "Programs", "CLPP", "stdlib"));
  }
  return roots;
}

function resolveInclude(spec, angled, fromPath, folders) {
  return resolveOnDisk(spec, angled, fromPath, folders);
}

function extraIncludeTexts(text, fromPath, folders) {
  const texts = [];
  const seen = new Set();
  const visit = (body, currentPath) => {
    const includeRe = /#include\s+(?:"([^"]+)"|<([^>]+)>)/g;
    let match;
    while ((match = includeRe.exec(body))) {
      const spec = match[1] || match[2];
      const resolved = resolveInclude(spec, Boolean(match[2]), currentPath, folders);
      if (!resolved || seen.has(resolved)) {
        continue;
      }
      seen.add(resolved);
      try {
        const next = fs.readFileSync(resolved, "utf8");
        texts.push(next);
        visit(next, resolved);
      } catch {
        // missing include — skip
      }
    }
  };
  visit(text, fromPath || null);
  return texts;
}

function eachInclude(text, fromPath, folders) {
  const re = /#include\s+(?:"([^"]+)"|<([^>]+)>)/g;
  const out = [];
  let match;
  while ((match = re.exec(text))) {
    const spec = match[1] || match[2];
    const specIndex = match.index + match[0].lastIndexOf(spec);
    out.push({
      spec,
      angled: Boolean(match[2]),
      resolved: resolveInclude(spec, Boolean(match[2]), fromPath, folders),
      start: match.index,
      end: match.index + match[0].length,
      specStart: specIndex,
      specEnd: specIndex + spec.length,
    });
  }
  return out;
}

function walkSourceFiles(dir, out, depth) {
  if (depth > 8 || !dir) {
    return;
  }
  let entries;
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch {
    return;
  }
  for (const entry of entries) {
    if (SKIP.has(entry.name) || entry.name.startsWith(".")) {
      continue;
    }
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      walkSourceFiles(full, out, depth + 1);
      continue;
    }
    if (SOURCE_EXT.has(path.extname(entry.name))) {
      out.push(full);
    }
  }
}

function indexMethods(folders) {
  const methods = [];
  const files = [];
  for (const folder of folders || []) {
    walkSourceFiles(folder, files, 0);
  }
  const re = /\b([A-Za-z_]\w*)::([A-Za-z_]\w*)\s*\(/g;
  for (const file of files) {
    let text;
    try {
      text = fs.readFileSync(file, "utf8");
    } catch {
      continue;
    }
    let match;
    while ((match = re.exec(text))) {
      const before = text.slice(0, match.index);
      const line = before.split(/\r?\n/).length - 1;
      const lineStart = before.lastIndexOf("\n") + 1;
      methods.push({
        owner: match[1],
        name: match[2],
        file,
        line,
        character: match.index - lineStart + match[0].lastIndexOf(match[2]),
      });
    }
  }
  return methods;
}

function offsetToPosition(text, offset) {
  const before = text.slice(0, offset);
  const lines = before.split(/\r?\n/);
  return { line: lines.length - 1, character: lines[lines.length - 1].length };
}

function positionToOffset(text, position) {
  const lines = text.split(/\r?\n/);
  let offset = 0;
  for (let i = 0; i < position.line; i++) {
    offset += (lines[i] || "").length + 1;
  }
  return offset + (position.character || 0);
}

function wordAt(text, position) {
  const offset = positionToOffset(text, position);
  const re = /@[A-Za-z_][A-Za-z0-9_]*|[A-Za-z_][A-Za-z0-9_]*/g;
  let match;
  while ((match = re.exec(text))) {
    if (offset >= match.index && offset <= match.index + match[0].length) {
      return {
        word: match[0],
        start: offsetToPosition(text, match.index),
        end: offsetToPosition(text, match.index + match[0].length),
      };
    }
  }
  return null;
}

function findDefinitions(text, filePath, position, folders, methodIndex) {
  const locations = [];
  const offset = positionToOffset(text, position);
  for (const inc of eachInclude(text, filePath, folders)) {
    if (offset >= inc.specStart && offset <= inc.specEnd && inc.resolved) {
      locations.push({
        uri: pathToUri(inc.resolved),
        range: {
          start: { line: 0, character: 0 },
          end: { line: 0, character: 0 },
        },
      });
    }
  }
  const hit = wordAt(text, position);
  if (!hit) {
    return locations;
  }
  const word = hit.word.replace(/^@/, "");
  const around = text.slice(Math.max(0, offset - 80), offset + 80);
  const qualified = around.match(new RegExp(`\\b([A-Za-z_]\\w*)::${word}\\b`));
  const owner = qualified ? qualified[1] : null;
  for (const method of methodIndex || []) {
    if (method.name !== word) {
      continue;
    }
    if (owner && method.owner !== owner) {
      continue;
    }
    locations.push({
      uri: pathToUri(method.file),
      range: {
        start: { line: method.line, character: method.character },
        end: { line: method.line, character: method.character + method.name.length },
      },
    });
  }
  return locations;
}

function pathToUri(filePath) {
  let normalized = path.resolve(filePath).replace(/\\/g, "/");
  if (!normalized.startsWith("/")) {
    normalized = `/${normalized}`;
  }
  return encodeURI(`file://${normalized}`).replace(/[?#]/g, encodeURIComponent);
}

function uriToPath(uri) {
  if (!uri) {
    return null;
  }
  let s = String(uri);
  if (s.startsWith("file://")) {
    s = decodeURIComponent(s.slice("file://".length));
    if (/^\/[A-Za-z]:/.test(s)) {
      s = s.slice(1);
    }
  }
  return s.replace(/\//g, path.sep);
}

module.exports = {
  includeSearchRoots,
  resolveInclude,
  extraIncludeTexts,
  eachInclude,
  indexMethods,
  findDefinitions,
  wordAt,
  pathToUri,
  uriToPath,
  positionToOffset,
};
