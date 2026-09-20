#!/usr/bin/env node
"use strict";

const { spawn } = require("child_process");
const child = spawn("clpp", ["lsp"], { stdio: "inherit", windowsHide: true });
child.on("error", (err) => {
  console.error("clpp lsp failed:", err.message);
  process.exit(1);
});
child.on("exit", (code) => process.exit(code ?? 1));
