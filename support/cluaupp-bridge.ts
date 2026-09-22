/**
 * Cluaupp host bridge — consumes CompileArtifact from `clpp`.
 * This file is the reference implementation contract for a separate Cluaupp package.
 */

import type { CompileArtifact } from "../cluaupp";

/** Apply selective @native only for hinted function names (never blanket). */
export function applyNativeHints(luau: string, artifact: CompileArtifact): string {
  const hints = new Set(artifact.nativeHints ?? []);
  if (hints.size === 0) return luau;
  const lines = luau.split("\n");
  const out: string[] = [];
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const m = line.match(/^(\s*)(local\s+function|function)\s+([A-Za-z_][\w]*)/);
    if (m && hints.has(m[3]) && (i === 0 || !lines[i - 1].includes("@native"))) {
      out.push(`${m[1]}@native`);
    }
    out.push(line);
  }
  return out.join("\n");
}

/** Decide SoA / buffer post-process from layoutHints. */
export function planLayout(artifact: CompileArtifact): {
  soa: string[];
  buffer: string[];
} {
  const soa: string[] = [];
  const buffer: string[] = [];
  for (const h of artifact.layoutHints ?? []) {
    if (h.startsWith("SoA:") || h.startsWith("SoACandidate:")) {
      soa.push(h.replace(/^SoA(Candidate)?:/, ""));
    } else if (
      h.startsWith("BufferSpecialize:") ||
      h.startsWith("BufferCandidate:") ||
      h.startsWith("DenseNumeric:")
    ) {
      buffer.push(h.split(":")[1] ?? h);
    }
  }
  return { soa, buffer };
}

/** PGO: merge profile JSON into next `clpp` / Cluaupp build --profile. */
export interface ProfileSample {
  function: string;
  calls: number;
  time_ms: number;
}

export function hotFunctions(samples: ProfileSample[], threshold = 0.05): string[] {
  const total = samples.reduce((a, s) => a + s.time_ms, 0) || 1;
  return samples
    .filter((s) => s.time_ms / total >= threshold)
    .map((s) => s.function);
}

export interface CluauppDoctorReport {
  ok: boolean;
  issues: string[];
}

/** Product surface stubs: watch / Rojo / remotes / DataModel / doctor / source maps. */
export const CluauppProduct = {
  watch(_roots: string[]): void {
    /* host: fs.watch → recompile */
  },
  syncRojo(_projectJson: string): void {
    /* host: map rojoClass from artifact */
  },
  typedRemote(_name: string, _payload: unknown): void {
    /* host: RemoteEvent wrap with CL++ types */
  },
  dataModelQuery(_path: string): unknown {
    return null;
  },
  doctor(artifact: CompileArtifact): CluauppDoctorReport {
    const issues: string[] = [];
    if (!artifact.ok) issues.push("compile failed");
    if (!(artifact.optimized ?? true)) issues.push("compiled with --no-opt");
    return { ok: issues.length === 0, issues };
  },
  attachSourceMap(artifact: CompileArtifact): CompileArtifact["sourceMap"] {
    return artifact.sourceMap ?? [];
  },
};
