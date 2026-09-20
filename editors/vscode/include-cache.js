"use strict";

const fs = require("fs");
const path = require("path");

const SOURCE_EXT = new Set([".clpp", ".clp", ".clh"]);

const fileTexts = new Map();
const extrasByDoc = new Map();
let timer = null;
let lastJob = null;

function extrasFor(fromPath) {
  return extrasByDoc.get(fromPath || "") || [];
}

function cachedText(filePath) {
  return fileTexts.get(filePath) || null;
}

function normalizeSpec(spec) {
  return String(spec || "")
    .replace(/\\/g, "/")
    .replace(/^\.\//, "");
}

function strippedSpec(spec) {
  let out = normalizeSpec(spec);
  while (out.startsWith("../")) {
    out = out.slice(3);
  }
  return out.replace(/^\.\//, "");
}

function pushUnique(out, seen, candidate) {
  if (!candidate) {
    return;
  }
  const key = path.normalize(candidate);
  if (seen.has(key)) {
    return;
  }
  seen.add(key);
  out.push(key);
}

function withSourceExts(candidate) {
  const ext = path.extname(candidate);
  if (ext && SOURCE_EXT.has(ext)) {
    return [candidate];
  }
  if (ext) {
    return [candidate];
  }
  return [candidate, `${candidate}.clh`, `${candidate}.clp`, `${candidate}.clpp`];
}

function candidatePaths(spec, angled, fromPath, folders) {
  const out = [];
  const seen = new Set();
  const specNorm = normalizeSpec(spec);
  const stripped = strippedSpec(specNorm);
  const fromSrc = specNorm.replace(/^src[/]/, "");
  const dir = fromPath ? path.dirname(fromPath) : "";

  const add = (base, rest) => {
    if (!base || !rest) {
      return;
    }
    for (const candidate of withSourceExts(path.join(base, rest))) {
      pushUnique(out, seen, candidate);
    }
  };

  if (!angled && dir) {
    add(dir, specNorm);
    add(dir, stripped);
  }
  if (dir) {
    let walk = dir;
    for (let i = 0; i < 12; i++) {
      add(walk, specNorm);
      add(walk, stripped);
      add(path.join(walk, "src"), specNorm);
      add(path.join(walk, "src"), stripped);
      add(path.join(walk, "src"), fromSrc);
      add(path.join(walk, "include"), specNorm);
      add(path.join(walk, "include"), stripped);
      const parent = path.dirname(walk);
      if (parent === walk) {
        break;
      }
      walk = parent;
    }
  }
  for (const folder of folders || []) {
    add(folder, specNorm);
    add(folder, stripped);
    add(path.join(folder, "src"), specNorm);
    add(path.join(folder, "src"), stripped);
    add(path.join(folder, "src"), fromSrc);
    add(path.join(folder, "include"), specNorm);
    add(path.join(folder, "include"), stripped);
    add(path.join(folder, "stdlib"), specNorm);
  }
  const local = process.env.LOCALAPPDATA || "";
  if (local) {
    add(path.join(local, "Programs", "CLPP"), specNorm);
    add(path.join(local, "Programs", "CLPP", "stdlib"), specNorm);
  }
  return out;
}

function resolveOnDisk(spec, angled, fromPath, folders) {
  for (const candidate of candidatePaths(spec, angled, fromPath, folders)) {
    try {
      if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) {
        return candidate;
      }
    } catch {
      // try next
    }
  }
  return null;
}

function resolveCached(spec, angled, fromPath, folders) {
  for (const candidate of candidatePaths(spec, angled, fromPath, folders)) {
    if (fileTexts.has(candidate)) {
      return candidate;
    }
  }
  return resolveOnDisk(spec, angled, fromPath, folders);
}

function scheduleLoad(text, fromPath, folders) {
  lastJob = { text, fromPath: fromPath || "", folders: folders || [] };
  if (timer) {
    return;
  }
  timer = setTimeout(() => {
    timer = null;
    const job = lastJob;
    lastJob = null;
    if (job) {
      loadIncludes(job.text, job.fromPath, job.folders).catch(() => {});
    }
  }, 250);
}

async function loadIncludes(text, fromPath, folders) {
  const texts = [];
  const seen = new Set();
  const visit = async (body, currentPath) => {
    const includeRe = /#include\s+(?:"([^"]+)"|<([^>]+)>)/g;
    let match;
    while ((match = includeRe.exec(body))) {
      const spec = match[1] || match[2];
      const resolved = await resolveAsync(spec, Boolean(match[2]), currentPath, folders);
      if (!resolved || seen.has(resolved)) {
        continue;
      }
      seen.add(resolved);
      let next = fileTexts.get(resolved);
      if (next === undefined) {
        try {
          next = await fs.promises.readFile(resolved, "utf8");
          fileTexts.set(resolved, next);
        } catch {
          continue;
        }
      }
      texts.push(next);
      await visit(next, resolved);
    }
  };
  await visit(text, fromPath);
  extrasByDoc.set(fromPath || "", texts);
}

async function resolveAsync(spec, angled, fromPath, folders) {
  for (const candidate of candidatePaths(spec, angled, fromPath, folders)) {
    if (fileTexts.has(candidate)) {
      return candidate;
    }
    try {
      await fs.promises.access(candidate);
      const st = await fs.promises.stat(candidate);
      if (st.isFile()) {
        return candidate;
      }
    } catch {
      // try next candidate
    }
  }
  return null;
}

function projectSrcRoots(fromPath, folders) {
  const roots = [];
  const seen = new Set();
  const add = (dir) => {
    if (!dir || seen.has(dir)) {
      return;
    }
    seen.add(dir);
    roots.push(dir);
  };
  if (fromPath) {
    let walk = path.dirname(fromPath);
    for (let i = 0; i < 12; i++) {
      add(walk);
      add(path.join(walk, "src"));
      const parent = path.dirname(walk);
      if (parent === walk) {
        break;
      }
      walk = parent;
    }
  }
  for (const folder of folders || []) {
    add(folder);
    add(path.join(folder, "src"));
  }
  return roots;
}

function includePathCompletions(line, fromPath, folders) {
  const match = line.match(/#include\s+(?:"([^"]*)|<([^>]*))$/);
  if (!match) {
    return [];
  }
  const typed = (match[1] || match[2] || "").replace(/\\/g, "/");
  const dir = fromPath ? path.dirname(fromPath) : (folders && folders[0]) || "";
  const items = [];
  const seen = new Set();
  const add = (label, detail) => {
    const normalized = label.replace(/\\/g, "/");
    if (seen.has(normalized)) {
      return;
    }
    if (typed && !normalized.startsWith(typed) && !normalized.endsWith(typed)) {
      return;
    }
    seen.add(normalized);
    items.push({
      label: normalized,
      kind: "File",
      detail: detail || "include",
    });
  };

  const listDir = (root, prefix) => {
    let entries = [];
    try {
      entries = fs.readdirSync(root, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entry of entries) {
      if (entry.name.startsWith(".")) {
        continue;
      }
      const full = path.join(root, entry.name);
      if (entry.isDirectory()) {
        add(`${prefix}${entry.name}/`, "folder");
        continue;
      }
      if (!SOURCE_EXT.has(path.extname(entry.name))) {
        continue;
      }
      add(`${prefix}${entry.name}`, entry.name);
    }
  };

  const typedDir = typed.includes("/") ? typed.slice(0, typed.lastIndexOf("/") + 1) : "";
  const roots = projectSrcRoots(fromPath, folders);
  for (const root of roots) {
    const target = typedDir ? path.join(root, typedDir) : root;
    const prefix = typedDir;
    listDir(target, prefix);
    if (dir) {
      try {
        const rel = path.relative(dir, target).replace(/\\/g, "/");
        if (rel && rel !== "." && !rel.startsWith("..")) {
          listDir(target, rel.endsWith("/") ? rel : `${rel}/`);
        } else if (rel.startsWith("..")) {
          listDir(target, rel.endsWith("/") ? `${rel}/` : `${rel}/`);
        }
      } catch {
        // ignore relative errors
      }
    }
  }

  add("src/", "project src");
  if (typed.startsWith("src/") || typed === "src" || typed === "s") {
    for (const root of roots) {
      if (path.basename(root) === "src") {
        listDir(root, "src/");
      }
    }
  }
  return items.slice(0, 80);
}

module.exports = {
  extrasFor,
  cachedText,
  resolveCached,
  resolveOnDisk,
  scheduleLoad,
  candidatePaths,
  includePathCompletions,
};
