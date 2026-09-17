#!/usr/bin/env node
"use strict";

/**
 * Runs Moonwave and injects the CL++ Prism / Refractor language id.
 *
 * Moonwave only copies `.moonwave/static`, `custom.css`, and `sidebars.js`.
 * It does not swizzle Prism, and `@mapbox/rehype-prism` (API tab) uses
 * Refractor, which throws on unknown fence ids such as `clpp`.
 */

const fs = require("fs");
const path = require("path");
const os = require("os");
const { spawn } = require("child_process");

const ROOT = path.resolve(__dirname, "..");
const THEME_SRC = path.join(
  ROOT,
  ".moonwave",
  "src",
  "theme",
  "prism-include-languages.js"
);

function moonwaveCacheDir() {
  const id = path.basename(process.cwd());
  let cacheHome;
  if (process.platform === "win32") {
    cacheHome = path.join(
      process.env.LOCALAPPDATA || path.join(os.homedir(), "AppData", "Local"),
      "moonwave",
      "Cache"
    );
  } else if (process.platform === "darwin") {
    cacheHome = path.join(os.homedir(), "Library", "Caches", "moonwave");
  } else {
    cacheHome = path.join(
      process.env.XDG_CACHE_HOME || path.join(os.homedir(), ".cache"),
      "moonwave"
    );
  }
  return path.join(cacheHome, id);
}

function inject() {
  const cache = moonwaveCacheDir();
  if (!fs.existsSync(cache)) {
    return false;
  }

  if (fs.existsSync(THEME_SRC)) {
    const destDir = path.join(cache, "src", "theme");
    fs.mkdirSync(destDir, { recursive: true });
    fs.copyFileSync(
      THEME_SRC,
      path.join(destDir, "prism-include-languages.js")
    );
  }

  const cfgPath = path.join(cache, "docusaurus.config.js");
  if (fs.existsSync(cfgPath)) {
    let text = fs.readFileSync(cfgPath, "utf8");
    if (text.includes("additionalLanguages") && !text.includes('"cpp"')) {
      text = text.replace(
        '"additionalLanguages": [',
        '"additionalLanguages": [\n          "c",\n          "cpp",'
      );
      fs.writeFileSync(cfgPath, text);
    }
  }

  const mdPath = path.join(
    cache,
    "node_modules",
    "docusaurus-plugin-moonwave",
    "src",
    "components",
    "Markdown.js"
  );
  if (fs.existsSync(mdPath)) {
    let md = fs.readFileSync(mdPath, "utf8");
    if (md.includes(".use(rehypePrism)") && !md.includes("alias: { cpp:")) {
      md = md.replace(
        ".use(rehypePrism)",
        '.use(rehypePrism, { alias: { cpp: ["clpp", "clp", "clh"] } })'
      );
      fs.writeFileSync(mdPath, md);
    }
  }

  return true;
}

const args = process.argv.slice(2);
const command = process.platform === "win32" ? "npx.cmd" : "npx";
const child = spawn(command, ["--yes", "moonwave@latest", ...args], {
  cwd: ROOT,
  stdio: "inherit",
  shell: true,
});

const timer = setInterval(inject, 200);
inject();

child.on("exit", (code) => {
  inject();
  clearInterval(timer);
  process.exit(code == null ? 1 : code);
});
