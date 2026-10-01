import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";
import { sidebar } from "./sidebar.js";

// The CL++ TextMate grammar highlights ```clp code blocks, so the website and the editor agree.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const grammar = JSON.parse(
  fs.readFileSync(path.join(root, "tools/vscode/syntaxes/clpp.tmLanguage.json"), "utf8")
);
grammar.name = "clpp";
grammar.scopeName = grammar.scopeName || "source.clpp";
grammar.aliases = ["clp"];

export default defineConfig({
  site: "https://kartzrbx.github.io",
  base: "/CLPP/",
  srcDir: "./src",
  integrations: [
    starlight({
      title: "CL++",
      description:
        "A compiled, general-purpose language made for games: static types with inference, no headers, and a built-in window, 2D graphics, UI, audio and automation.",
      favicon: "/favicon.png",
      logo: { src: "./src/assets/logo.png", alt: "CL++", replacesTitle: false },
      social: [{ icon: "github", label: "GitHub", href: "https://github.com/KartzRbx/CLPP" }],
      customCss: ["./src/styles/custom.css"],
      expressiveCode: { shiki: { langs: [grammar] } },
      sidebar,
      head: [{ tag: "meta", attrs: { name: "theme-color", content: "#F5B700" } }],
    }),
  ],
});
