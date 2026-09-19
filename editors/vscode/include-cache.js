"use strict";

const fs = require("fs");
const path = require("path");

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

function candidatePaths(spec, angled, fromPath, folders) {
  const out = [];
  const seen = new Set();
  const push = (candidate) => {
    if (!candidate || seen.has(candidate)) {
      return;
    }
    seen.add(candidate);
    out.push(candidate);
  };
  if (!angled && fromPath) {
    push(path.normalize(path.join(path.dirname(fromPath), spec)));
  }
  for (const folder of folders || []) {
    push(path.join(folder, spec));
    push(path.join(folder, "stdlib", spec));
  }
  const local = process.env.LOCALAPPDATA || "";
  if (local) {
    push(path.join(local, "Programs", "CLPP", spec));
    push(path.join(local, "Programs", "CLPP", "stdlib", spec));
  }
  return out;
}

function resolveCached(spec, angled, fromPath, folders) {
  for (const candidate of candidatePaths(spec, angled, fromPath, folders)) {
    if (fileTexts.has(candidate)) {
      return candidate;
    }
  }
  return null;
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
      return candidate;
    } catch {
      // try next candidate
    }
  }
  return null;
}

module.exports = {
  extrasFor,
  cachedText,
  resolveCached,
  scheduleLoad,
  candidatePaths,
};
