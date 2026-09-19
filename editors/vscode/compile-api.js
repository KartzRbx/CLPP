"use strict";

const fs = require("fs");
const path = require("path");
const cp = require("child_process");

function findClpp(workspaceFolders) {
  const folders = workspaceFolders || [];
  const local = process.env.LOCALAPPDATA || "";
  const names = process.platform === "win32" ? ["clpp.exe"] : ["clpp"];
  const extra = [path.join(local, "Programs", "CLPP", "clpp.exe")];
  for (const root of folders) {
    extra.push(path.join(root, "target", "release", names[0]));
    extra.push(path.join(root, "target", "debug", names[0]));
  }
  extra.push(path.join(__dirname, "..", "..", "target", "release", names[0]));
  extra.push(path.join(__dirname, "..", "..", "target", "debug", names[0]));
  for (const candidate of extra) {
    if (candidate && fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return names[0];
}

function compileDiagnostics(source, fileName, workspaceFolders) {
  const exe = findClpp(workspaceFolders);
  const payload = JSON.stringify({
    source,
    fileName: fileName || "untitled.clpp",
  });
  try {
    const result = cp.spawnSync(exe, ["api", "compile"], {
      input: payload,
      encoding: "utf8",
      timeout: 8000,
      windowsHide: true,
    });
    if (result.error) {
      return null;
    }
    const out = (result.stdout || "").trim();
    if (!out) {
      return null;
    }
    const art = JSON.parse(out);
    const items = [];
    for (const d of art.diagnostics || []) {
      items.push({
        line: Math.max(0, (d.line || 1) - 1),
        column: Math.max(0, (d.column || 1) - 1),
        message: d.message,
      });
    }
    if (!items.length && art.ok === false && art.error) {
      const match = String(art.error).match(/[:\[](\d+)[:.](\d+)/);
      items.push({
        line: match ? Math.max(0, Number(match[1]) - 1) : 0,
        column: match ? Math.max(0, Number(match[2]) - 1) : 0,
        message:
          String(art.error)
            .split("\n")
            .map((l) => l.trim())
            .find(
              (l) =>
                l &&
                !l.startsWith("╭") &&
                !l.startsWith("│") &&
                !l.startsWith("╰") &&
                !l.startsWith("→")
            ) || "CL++ error",
      });
    }
    return items;
  } catch {
    return null;
  }
}

function mergeIssues(compiler, lint) {
  const items = compiler ? [...compiler] : [];
  const seen = new Set(items.map((i) => `${i.line}:${i.message}`));
  for (const issue of lint || []) {
    const key = `${issue.line}:${issue.message}`;
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    items.push(issue);
  }
  return items;
}

module.exports = { findClpp, compileDiagnostics, mergeIssues };
