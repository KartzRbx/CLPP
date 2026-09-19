"use strict";

const fs = require("fs");
const path = require("path");
const cp = require("child_process");

let cachedExe = "";

function findClpp(workspaceFolders) {
  if (cachedExe && fs.existsSync(cachedExe)) {
    return cachedExe;
  }
  const folders = workspaceFolders || [];
  const home = process.env.USERPROFILE || process.env.HOME || "";
  const local = process.env.LOCALAPPDATA || "";
  const names = process.platform === "win32" ? ["clpp.exe"] : ["clpp"];
  const extra = [
    path.join(local, "Programs", "CLPP", "clpp.exe"),
    path.join(home, ".cargo", "bin", names[0]),
  ];
  if (process.env.CLPP_DEV_COMPILER) {
    for (const root of folders) {
      extra.push(path.join(root, "target", "release", names[0]));
      extra.push(path.join(root, "target", "debug", names[0]));
    }
    extra.push(path.join(__dirname, "..", "..", "target", "release", names[0]));
    extra.push(path.join(__dirname, "..", "..", "target", "debug", names[0]));
  }
  for (const candidate of extra) {
    if (candidate && fs.existsSync(candidate)) {
      cachedExe = candidate;
      return candidate;
    }
  }
  cachedExe = names[0];
  return cachedExe;
}

function parseCompileOutput(out) {
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
      timeout: 4000,
      windowsHide: true,
    });
    if (result.error) {
      return null;
    }
    const out = (result.stdout || "").trim();
    if (!out) {
      return null;
    }
    return parseCompileOutput(out);
  } catch {
    return null;
  }
}

function compileDiagnosticsAsync(source, fileName, workspaceFolders, done) {
  const exe = findClpp(workspaceFolders);
  const payload = JSON.stringify({
    source,
    fileName: fileName || "untitled.clpp",
  });
  let settled = false;
  const finish = (items) => {
    if (settled) {
      return;
    }
    settled = true;
    done(items);
  };
  try {
    const child = cp.spawn(exe, ["api", "compile"], {
      windowsHide: true,
    });
    let out = "";
    const timer = setTimeout(() => {
      try {
        child.kill();
      } catch {
        // ignore
      }
      finish(null);
    }, 4000);
    child.stdout.setEncoding("utf8");
    child.stdout.on("data", (chunk) => {
      out += chunk;
    });
    child.on("error", () => {
      clearTimeout(timer);
      finish(null);
    });
    child.on("close", () => {
      clearTimeout(timer);
      const text = out.trim();
      if (!text) {
        finish(null);
        return;
      }
      try {
        finish(parseCompileOutput(text));
      } catch {
        finish(null);
      }
    });
    child.stdin.write(payload);
    child.stdin.end();
  } catch {
    finish(null);
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

module.exports = { findClpp, compileDiagnostics, compileDiagnosticsAsync, mergeIssues };
