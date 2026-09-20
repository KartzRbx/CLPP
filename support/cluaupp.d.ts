/** Stable CL++ ↔ Cluaupp contract (CL++ 0.4.0). Cluaupp invokes the `clpp` binary.
 *  Full CLI handoff: docs/cluaupp-032.md
 *  Anonymous callbacks in generated CL++ must be `func (params) { }`.
 *  See docs/cluaupp-callbacks.md — `func [](…)` / `[]() { }` do not compile.
 */

export interface CompileRequest {
  source: string;
  fileName: string;
  strict?: boolean;
}

export interface CompileDiagnostic {
  message: string;
  /** 1-based */
  line: number;
  /** 1-based */
  column: number;
  severity: string;
}

export interface CompileArtifact {
  ok: boolean;
  luau: string;
  fileName: string;
  outputHint: string;
  scriptKind: "server" | "client" | "plugin" | null;
  isScript: boolean;
  isHeader: boolean;
  rojoClass: "Script" | "LocalScript" | "ModuleScript";
  libraries: string[];
  error?: string;
  diagnostics?: CompileDiagnostic[];
}

export interface LanguageManifest {
  name: "CL++";
  id: "clpp";
  version: string;
  extensions: string[];
  tags: Array<{
    pattern: string;
    scriptKind: string;
    rojoClass: string;
  }>;
  builtins: string[];
  operators: Array<{ clpp: string; luau: string; meaning: string }>;
  io: Array<{ clpp: string; luau: string }>;
}

/**
 * CLI
 *
 *   clpp --version                  // must be 0.4.0
 *   clpp api compile --file path.server.clpp
 *   echo '{"source":"...","fileName":"x.server.clpp"}' | clpp api compile
 *   clpp api manifest
 *   clpp api complete | hover | symbols | definition
 *   clpp compile file.clpp --json
 *   clpp fmt file.clpp
 *   clpp install
 */
export {};
