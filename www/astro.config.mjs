import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";
import { sidebar } from "./sidebar.js";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const grammar = JSON.parse(
  fs.readFileSync(
    path.join(root, "editors/vscode/syntaxes/clpp.tmLanguage.json"),
    "utf8"
  )
);
grammar.name = "clpp";
grammar.id = "clpp";
grammar.aliases = ["clp", "clh"];

export default defineConfig({
  site: "https://kartzrbx.github.io",
  base: "/CLPP/",
  srcDir: "./src",
  integrations: [
    starlight({
      title: "CL++",
      description:
        "A C++-inspired language that compiles to Luau. Course, reference, and specification.",
      favicon: "/favicon.png",
      logo: {
        src: "./src/assets/logo.png",
        alt: "CL++",
        replacesTitle: false,
      },
      social: [
        {
          icon: "github",
          label: "GitHub",
          href: "https://github.com/KartzRbx/CLPP",
        },
      ],
      editLink: {
        baseUrl: "https://github.com/KartzRbx/CLPP/edit/main/",
      },
      customCss: ["./src/styles/custom.css"],
      components: {
        Head: "./src/components/Head.astro",
      },
      expressiveCode: {
        shiki: {
          langs: [grammar],
        },
      },
      sidebar,
      head: [
        {
          tag: "meta",
          attrs: { name: "theme-color", content: "#7c3aed" },
        },
      ],
    }),
  ],
  redirects: {
    "/api": "/docs/reference",
    "/api/Builtins": "/docs/reference",
    "/api/Types": "/docs/spec/types",
    "/api/Operators": "/docs/operators",
    "/api/Functions": "/docs/functions",
    "/api/OOP": "/docs/oop",
    "/api/ControlFlow": "/docs/control-flow",
    "/api/Collections": "/docs/collections",
    "/api/Signals": "/docs/observables-signals",
    "/api/Observables": "/docs/observables-signals",
    "/api/Concurrency": "/docs/concurrency",
    "/api/Attributes": "/docs/attributes",
    "/api/Preprocessor": "/docs/files",
    "/api/Stdlib": "/docs/reference/header-roblox",
  },
});
