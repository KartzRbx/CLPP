"use strict";

function createLens(vscode) {
  const error = vscode.window.createTextEditorDecorationType({
    isWholeLine: true,
    backgroundColor: "rgba(255, 45, 85, 0.14)",
    overviewRulerColor: "#ff2d55",
    overviewRulerLane: vscode.OverviewRulerLane.Right,
  });
  return { error };
}

function applyLens(vscode, types, editor, diagnostics) {
  if (!editor || (editor.document.languageId !== "clpp" && editor.document.languageId !== "clpp-header")) {
    return;
  }
  const enabled = vscode.workspace.getConfiguration("clpp").get("lens.enabled", true);
  if (!enabled || !diagnostics.length) {
    editor.setDecorations(types.error, []);
    return;
  }
  const byLine = new Map();
  for (const diagnostic of diagnostics) {
    const line = diagnostic.range.start.line;
    const list = byLine.get(line) || [];
    if (!list.includes(diagnostic.message)) {
      list.push(diagnostic.message);
    }
    byLine.set(line, list);
  }
  const decorations = [];
  for (const [line, messages] of byLine) {
    if (line >= editor.document.lineCount) {
      continue;
    }
    const range = editor.document.lineAt(line).range;
    decorations.push({
      range,
      renderOptions: {
        after: {
          margin: "0 0 0 1.6em",
          color: "#ff5c7a",
          fontStyle: "italic",
          contentText: `  ←  ${messages.join(" · ")}`,
        },
      },
    });
  }
  editor.setDecorations(types.error, decorations);
}

function applyLensToOpenEditors(vscode, types, collection) {
  for (const editor of vscode.window.visibleTextEditors) {
    const found = collection.get(editor.document.uri) || [];
    applyLens(vscode, types, editor, found);
  }
}

module.exports = { createLens, applyLens, applyLensToOpenEditors };
