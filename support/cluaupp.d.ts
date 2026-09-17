/** Stable CL++ ↔ Cluaupp contract. Cluaupp invokes the `clpp` binary. */

export interface CompileRequest {
  source: string;
  fileName: string;
  strict?: boolean;
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
 *   clpp api compile --file path.server.clpp
 *   echo '{"source":"...","fileName":"x.server.clpp"}' | clpp api compile
 *   clpp api manifest
 *   clpp compile file.clpp --json
 *   clpp install
 */
export {};
