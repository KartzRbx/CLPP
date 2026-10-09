#!/usr/bin/env node

/**
 * Copy docs/*.md into the Starlight content collection so URLs stay
 * /docs/... (same as the old Moonwave site).
 */

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const SRC = path.join(ROOT, "docs");
const DEST = path.join(ROOT, "www", "src", "content", "docs", "docs");
const ASSETS = path.join(ROOT, "www", "src", "assets");
const PUBLIC = path.join(ROOT, "www", "public");

const SKIP = new Set([
  path.normalize("README.md"),
  path.normalize("tutorials/README.md"),
  path.normalize("design/README.md"),
  path.normalize("spec/README.md"),
]);

function walk(dir, acc = []) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      walk(full, acc);
    } else if (entry.name.endsWith(".md") || entry.name.endsWith(".mdx")) {
      acc.push(full);
    }
  }
  return acc;
}

function parseFile(text) {
  const match = text.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n?([\s\S]*)$/);
  if (!match) {
    return { data: {}, body: text };
  }
  const data = {};
  for (const line of match[1].split(/\r?\n/)) {
    const kv = line.match(/^([A-Za-z0-9_]+):\s*(.*)$/);
    if (!kv) {
      continue;
    }
    let value = kv[2].trim();
    if (
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"))
    ) {
      value = value.slice(1, -1);
    }
    data[kv[1]] = value;
  }
  return { data, body: match[2] };
}

function firstHeading(body) {
  const m = body.match(/^\s*#\s+(.+?)\s*$/m);
  return m ? m[1].replace(/`/g, "") : "";
}

function toFrontmatter(data) {
  const lines = ["---"];
  for (const [key, value] of Object.entries(data)) {
    if (value === undefined || value === "") {
      continue;
    }
    if (typeof value === "object") {
      lines.push(`${key}:`);
      for (const [k, v] of Object.entries(value)) {
        lines.push(`  ${k}: ${JSON.stringify(v)}`);
      }
    } else {
      lines.push(`${key}: ${JSON.stringify(String(value))}`);
    }
  }
  lines.push("---", "");
  return lines.join("\n");
}

function mermaidToHtml(body) {
  return body.replace(/```mermaid\r?\n([\s\S]*?)```/g, (_, code) => {
    const escaped = String(code)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;");
    return `<pre class="mermaid">${escaped}</pre>`;
  });
}

function stripMatchingH1(body, title) {
  if (!title) {
    return body;
  }
  const re = new RegExp(
    `^\\s*#\\s+${title.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}\\s*\\r?\\n+`
  );
  return body.replace(re, "");
}

const SITE_BASE = "/CLPP/luau/";

// Markdown is authored relative to source files; the output is a directory URL.
// Resolve links before copying so /types/ does not accidentally become /intro/types/.
function resolveDocLinks(body, rel) {
  const parts = body.split(/(```[\s\S]*?```)/g);
  return parts.map((part, i) => i % 2 ? part : part.replace(/(!?\[.*?\]\()([^\s)]+)([^)]*\))/g, (all, open, href, close) => {
    if (/^(?:[a-z]+:|#|\/)/i.test(href)) return all;
    const [target, fragment] = href.split('#');
    const source = path.resolve(SRC, path.dirname(rel), target);
    const candidates = [source, source + ".md", source + ".mdx"];
    const found = candidates.find(p => fs.existsSync(p));
    if (!found && path.relative(SRC, source) === "version") return open + SITE_BASE + "docs/version/" + (fragment ? "#" + fragment : "") + close;
    if (!found) return all;
    const inside = path.relative(SRC, found);
    const suffix = fragment ? "#" + fragment : "";
    if (!inside.startsWith("..") && /\.mdx?$/.test(found) && !SKIP.has(inside)) {
      return open + SITE_BASE + "docs/" + inside.split(path.sep).join('/').replace(/\.mdx?$/, '') + "/" + suffix + close;
    }
    const repo = path.relative(ROOT, found).split(path.sep).join('/');
    if (repo.startsWith('..')) return all;
    const kind = fs.statSync(found).isDirectory() ? 'tree' : 'blob';
    return open + "https://github.com/KartzRbx/CLPP/" + kind + "/v0.8.2/" + repo + suffix + close;
  })).join('');
}

function transform(rel, text) {
  const { data, body: rawBody } = parseFile(text);
  let body = rawBody.replace(/className=/g, "class=");
  const heading = firstHeading(body);
  const title = data.title || heading || path.basename(rel, path.extname(rel));
  body = stripMatchingH1(body, data.title);
  body = mermaidToHtml(body);
  body = resolveDocLinks(body, rel);

  const relPosix = rel.split(path.sep).join("/");
  const slug = `docs/${relPosix.replace(/\.mdx?$/, "")}`;
  const out = { title, slug };
  if (data.description) {
    out.description = data.description;
  }
  if (data.sidebar_label) {
    out.sidebar = { label: data.sidebar_label };
  }
  if (data.unlisted === "true" || data.unlisted === true) {
    out.draft = true;
  }
  if (rel.startsWith(`design${path.sep}`) || rel.startsWith("design/")) {
    out.sidebar = { ...(out.sidebar || {}), hidden: false };
  }
  return toFrontmatter(out) + body.replace(/^\uFEFF/, "").replace(/^\s+/, "");
}

function copyAssets() {
  fs.mkdirSync(ASSETS, { recursive: true });
  fs.mkdirSync(PUBLIC, { recursive: true });
  const logo = path.join(ROOT, ".moonwave", "static", "img", "logo.png");
  const favicon = path.join(ROOT, ".moonwave", "static", "img", "favicon.png");
  if (fs.existsSync(logo)) {
    fs.copyFileSync(logo, path.join(ASSETS, "logo.png"));
  }
  if (fs.existsSync(favicon)) {
    fs.copyFileSync(favicon, path.join(PUBLIC, "favicon.png"));
  }
}

function main() {
  fs.rmSync(DEST, { recursive: true, force: true });
  fs.mkdirSync(DEST, { recursive: true });
  copyAssets();
  const cargo = fs.readFileSync(path.join(ROOT, "Cargo.toml"), "utf8");
  const version = cargo.match(/^version = "([^"]+)"/m)[1];
  const support = fs.readFileSync(path.join(ROOT, "src/support.rs"), "utf8");
  const contract = support.match(/CONTRACT_VERSION: &str = "([^"]+)"/)[1];
  const versionDoc = ["---", "title: Version and contract", "---", "", "The Rust Luau compiler is **" + version + "** and the JSON contract is **" + contract + "**.", "", "This page is generated from Cargo.toml and src/support.rs on every documentation build. Run clpp api manifest to inspect the installed build. The C++20 VM is a separate product and uses clpp-vm.", ""].join("\n");
  fs.writeFileSync(path.join(DEST, "version.md"), transform("version.md", versionDoc));

  for (const file of walk(SRC)) {
    const rel = path.relative(SRC, file);
    if (SKIP.has(rel) || SKIP.has(rel.split(path.sep).join("/"))) {
      continue;
    }
    const dest = path.join(DEST, rel);
    fs.mkdirSync(path.dirname(dest), { recursive: true });
    const text = fs.readFileSync(file, "utf8");
    fs.writeFileSync(dest, transform(rel, text));
  }

  // Machine-readable bench output for the site / CI consumers.
  const benchJson = path.join(SRC, "benchmarks", "results.json");
  if (fs.existsSync(benchJson)) {
    const pubBench = path.join(PUBLIC, "benchmarks");
    fs.mkdirSync(pubBench, { recursive: true });
    fs.copyFileSync(benchJson, path.join(pubBench, "results.json"));
  }
}

main();
