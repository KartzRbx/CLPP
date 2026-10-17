// Generates Starlight pages and navigation from the English website source.
// It is also run automatically before `astro build` (see package.json "prebuild").

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const docs = path.resolve(here, "..", "docs");
const out = path.join(here, "src", "content", "docs");
const siteBase = "/CLPP/";
const handbook = fs.readdirSync(path.join(docs, "handbook")).filter((name) => name.endsWith(".md")).sort();
const editBase = "https://github.com/KartzRbx/CLPP/edit/main/docs/";

// Guide chapters, project tutorials, and language reference pages.
const guide = fs
  .readdirSync(path.join(docs, "guide"))
  .filter((name) => name.endsWith(".md"))
  .sort();
const referenceOrder = ["syntax.md", "status.md", "keywords.md", "def.md", "lexer.md", "comments.md", "identifiers.md", "literals.md", "punctuation.md", "ebnf.md", "toolchain.md"];
const reference = fs.readdirSync(path.join(docs, "reference"))
  .filter((name) => name.endsWith(".md"))
  .sort((a, b) => {
    const rank = (name) => referenceOrder.includes(name) ? referenceOrder.indexOf(name) : referenceOrder.length;
    return rank(a) - rank(b) || a.localeCompare(b);
  });

function firstHeading(text) {
  const match = text.match(/^#\s+(.+)$/m);
  return match ? match[1].trim() : "Untitled";
}

// Strip the chapter number and any `code` marks
// for the page title and sidebar label.
function shortLabel(title) {
  return title.replace(/^\d+\.\s*/, "").replace(/`/g, "").trim();
}

function escapeYaml(value) {
  return value.replace(/"/g, '\\"');
}

// Turn a repo doc into a Starlight page: add frontmatter, drop the first H1 (Starlight renders the
// title), and make code fences safe for the website's syntax highlighter.
function toPage(source, sourcePath) {
  const title = firstHeading(source);
  let body = source.replace(/^---\r?\n[\s\S]*?\r?\n---\r?\n/, "").replace(/^\s*#\s+.+\r?\n/, "");
  // `clp erro` / `clp trecho` / `clp file=...` -> `clp`; output fences -> text.
  body = body.replace(/```clp[^\n]*/g, "```clp");
  body = body.replace(/```(saida|output)\b/g, "```text");
  body = body.replace(/```ebnf\b/g, "```text");
  // Repository Markdown paths do not resolve to routes on the published site.
  body = body.replace(/\]\(([^)\s]+\.md)(#[^)]+)?\)/g, (match, href, fragment = "") => {
    if (/^(?:[a-z][a-z0-9+.-]*:|\/\/)/i.test(href)) return match;
    const target = path.relative(docs, path.resolve(path.dirname(sourcePath), href)).replaceAll("\\", "/");
    if (target.startsWith("../") || !target.endsWith(".md")) {
      throw new Error(`Unsupported documentation link in ${sourcePath}: ${href}`);
    }
    const slug = target.slice(0, -3).toLowerCase();
    return `](${siteBase}${slug}/${fragment})`;
  });
  const editPath = path.relative(docs, sourcePath).replaceAll("\\", "/");
  const frontmatter = `---\ntitle: "${escapeYaml(shortLabel(title))}"\neditUrl: "${editBase}${editPath}"\n---\n\n`;
  return { title, content: frontmatter + body };
}

function write(relativeOut, source, sourcePath) {
  const page = toPage(source, sourcePath);
  const target = path.join(out, relativeOut);
  fs.mkdirSync(path.dirname(target), { recursive: true });
  if (!fs.existsSync(target) || fs.readFileSync(target, "utf8") !== page.content) {
    fs.writeFileSync(target, page.content, "utf8");
  }
  return page.title;
}

// Remove pages whose source document no longer exists without touching unchanged pages.
for (const [sub, names] of [
  ["guide", guide.map((name) => name.toLowerCase())],
  ["handbook", handbook.map((name) => name.toLowerCase())],
  ["reference", reference.filter((name) => fs.existsSync(path.join(docs, "reference", name))).map((name) => name.toLowerCase())],
]) {
  const directory = path.join(out, sub);
  if (!fs.existsSync(directory)) continue;
  for (const name of fs.readdirSync(directory)) {
    if (name.endsWith(".md") && !names.includes(name.toLowerCase())) {
      fs.rmSync(path.join(directory, name));
    }
  }
}

const guideItems = [];
const startItems = [];
const libraryItems = [];
const toolsItems = [];
const concurrencyItems = [];
const projectItems = [];
for (const name of guide) {
  const sourcePath = path.join(docs, "guide", name);
  const source = fs.readFileSync(sourcePath, "utf8");
  const slugName = name.replace(/\.md$/, "");
  const title = write(path.join("guide", `${slugName}.md`), source, sourcePath);
  const item = { label: shortLabel(title), slug: `guide/${slugName}` };
  const chapter = Number.parseInt(slugName, 10);
  if (chapter <= 1) startItems.push(item);
  else if (chapter <= 11) guideItems.push(item);
  else if (chapter <= 13) concurrencyItems.push(item);
  else if ([14, 17, 18, 19].includes(chapter)) libraryItems.push(item);
  else if ([15, 16].includes(chapter)) toolsItems.push(item);
  else projectItems.push(item);
}

const referenceItems = [];
for (const name of reference) {
  const file = path.join(docs, "reference", name);
  if (!fs.existsSync(file)) {
    continue;
  }
  const source = fs.readFileSync(file, "utf8");
  const slugName = name.replace(/\.md$/, "").toLowerCase();
  const title = write(path.join("reference", `${slugName}.md`), source, file);
  referenceItems.push({ label: shortLabel(title), slug: `reference/${slugName}` });
}

const handbookItems = handbook.map((name) => {
  const sourcePath = path.join(docs, "handbook", name);
  const slugName = name.slice(0, -3);
  const title = write(path.join("handbook", name), fs.readFileSync(sourcePath, "utf8"), sourcePath);
  return { label: shortLabel(title), slug: `handbook/${slugName}` };
});

const groups = [
  { label: "Start here", items: startItems },
  { label: "Complete language handbook", items: handbookItems },
  { label: "Language syntax", items: guideItems },
  { label: "Lexer and language reference", items: referenceItems.filter((item) => item.slug !== "reference/toolchain") },
  { label: "Concurrency and events", items: concurrencyItems },
  { label: "Standard library and APIs", items: libraryItems },
  { label: "Editor, toolchain and performance", items: [...toolsItems, ...referenceItems.filter((item) => item.slug === "reference/toolchain")] },
  { label: "Complete projects", items: projectItems },
];
const sidebar = `// Generated by build-content.mjs from docs/. Do not edit by hand.
/** @type {import('@astrojs/starlight/types').StarlightUserConfig['sidebar']} */
export const sidebar = ${JSON.stringify(groups, null, 2)};
`;
const sidebarPath = path.join(here, "sidebar.js");
if (!fs.existsSync(sidebarPath) || fs.readFileSync(sidebarPath, "utf8") !== sidebar) {
  fs.writeFileSync(sidebarPath, sidebar, "utf8");
}

console.log(`Generated ${guide.length} English guide/project pages, ${handbook.length} handbook chapters and ${referenceItems.length} reference pages in ${groups.length} topic groups.`);
