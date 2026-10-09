/** Stable CL++ ↔ Cluaupp contract (CL++ 0.8.2). Cluaupp invokes the `clpp` binary.
 *  Full CLI handoff: docs/cluaupp-032.md
 *  Anonymous callbacks in generated CL++ must be `func (params) { }`.
 *  See docs/cluaupp-callbacks.md — `func [](…)` / `[]() { }` do not compile.
 */

export interface CompileRequest {
  source: string;
  fileName: string;
  strict?: boolean;
  /** false = skip high-level opts (baseline D). */
  optimize?: boolean;
  /** Dotted Luau path. Defaults to ReplicatedStorage.CluauppLibs. */
  libRoot?: string;
}

export interface CompileDiagnostic {
  message: string;
  /** 1-based */
  line: number;
  /** 1-based */
  column: number;
  severity: "error" | "warning";
  code: string;
  /** Suggested correction; retained under the existing help key. */
  help: string;
  span: SourceSpan;
}

export interface SourceSpan {
  startLine: number; startCol: number; endLine: number; endCol: number;
}

export interface SourceMapLine {
  luauLine: number; luauColumn: number; clppLine: number; clppColumn: number;
  file: string; span: SourceSpan;
}

export interface CompileArtifact {
  contractVersion: string;
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
  sourceMap?: SourceMapLine[];
  /** Selective @native candidates for Cluaupp (RFC 0011). Not auto-applied. */
  nativeHints?: string[];
  /** Monomorphized generic symbols (`name__Type`). */
  specialized?: string[];
  /** Dense / SoA / buffer layout candidates. */
  layoutHints?: string[];
  /** False when compiled with --no-opt / optimize:false. */
  optimized?: boolean;
}

export interface LanguageManifest {
  name: "CL++";
  id: "clpp";
  version: string;
  contractVersion: string;
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
 *   clpp --version                  // compiler version; inspect manifest.contractVersion for protocol compatibility
 *   clpp api compile --file path.server.clpp
 *   echo '{"source":"...","fileName":"x.server.clpp"}' | clpp api compile
 *   clpp api serve                 // NDJSON, one artifact per request, flush after each response
 *   clpp api manifest
 *   clpp api complete | hover | symbols | definition
 *   clpp compile file.clpp --json
 *   clpp fmt file.clpp
 *   clpp install
 */
export {};
