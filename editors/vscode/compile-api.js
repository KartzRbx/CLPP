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

function spawnClppJsonSync(cmd, args, payload, workspaceFolders, timeoutMs) {
  const exe = findClpp(workspaceFolders);
  try {
    const result = cp.spawnSync(exe, [cmd, ...args], {
      input: JSON.stringify(payload),
      encoding: "utf8",
      timeout: timeoutMs || 2000,
      windowsHide: true,
    });
    const out = (result.stdout || "").trim();
    if (!out) {
      return null;
    }
    return JSON.parse(out);
  } catch {
    return null;
  }
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

function spawnClppJson(cmd, args, payload, workspaceFolders, done) {
  const exe = findClpp(workspaceFolders);
  const body = JSON.stringify(payload);
  let settled = false;
  const finish = (value, error) => {
    if (settled) {
      return;
    }
    settled = true;
    done(value, error);
  };
  try {
    const child = cp.spawn(exe, [cmd, ...args], { windowsHide: true });
    let out = "";
    let err = "";
    const timer = setTimeout(() => {
      try {
        child.kill();
      } catch {
        // ignore
      }
      finish(null, {
        message: "CL++ compiler timed out (4s). Check that clpp is on PATH or set CLPP_DEV_COMPILER=1.",
      });
    }, 4000);
    child.stdout.setEncoding("utf8");
    child.stderr.setEncoding("utf8");
    child.stdout.on("data", (chunk) => {
      out += chunk;
    });
    child.stderr.on("data", (chunk) => {
      err += chunk;
    });
    child.on("error", (error) => {
      clearTimeout(timer);
      finish(null, {
        message: `CL++ compiler not found (${exe}). Run clpp setup or clpp install.`,
        error: String(error && error.message ? error.message : error),
      });
    });
    child.on("close", () => {
      clearTimeout(timer);
      const text = out.trim();
      if (!text) {
        finish(null, {
          message: err.trim() || "CL++ compiler returned no JSON.",
        });
        return;
      }
      try {
        finish(JSON.parse(text), null);
      } catch (parseErr) {
        finish(null, { message: "CL++ compiler returned invalid JSON." });
      }
    });
    child.stdin.write(body);
    child.stdin.end();
  } catch (error) {
    finish(null, {
      message: `CL++ compiler spawn failed: ${error && error.message ? error.message : error}`,
    });
  }
}

function compileDiagnosticsAsync(source, fileName, workspaceFolders, done) {
  spawnClppJson("api", ["compile"], { source, fileName: fileName || "untitled.clpp" }, workspaceFolders, (art, error) => {
    if (error) {
      done([
        {
          line: 0,
          column: 0,
          message: error.message,
        },
      ]);
      return;
    }
    if (!art) {
      done([
        {
          line: 0,
          column: 0,
          message: "CL++ compiler returned no result.",
        },
      ]);
      return;
    }
    try {
      done(parseCompileOutput(JSON.stringify(art)));
    } catch {
      done([
        {
          line: 0,
          column: 0,
          message: "CL++ compiler returned invalid diagnostics.",
        },
      ]);
    }
  });
}

function mergeIssues(compiler, lint) {
  if (compiler && compiler.length) {
    return compiler;
  }
  return lint || [];
}

module.exports = {
  findClpp,
  compileDiagnostics,
  compileDiagnosticsAsync,
  mergeIssues,
  spawnClppJson,
  spawnClppJsonSync,
};
