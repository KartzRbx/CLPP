"use strict";

const { spawnClppJson, spawnClppJsonSync, findClpp } = require("./compile-api");

function positionPayload(source, fileName, position) {
  return {
    source,
    fileName: fileName || "untitled.clpp",
    line: (position && position.line ? position.line : 0) + 1,
    column: (position && typeof position.character === "number" ? position.character : 0) + 1,
  };
}

function apiComplete(source, fileName, position, folders, done) {
  done(spawnClppJsonSync("api", ["complete"], positionPayload(source, fileName, position), folders));
}

function apiHover(source, fileName, position, folders, done) {
  done(spawnClppJsonSync("api", ["hover"], positionPayload(source, fileName, position), folders));
}

function apiSymbols(source, fileName, folders, done) {
  spawnClppJson(
    "api",
    ["symbols"],
    positionPayload(source, fileName, { line: 0, character: 0 }),
    folders,
    done
  );
}

function apiDefinition(source, fileName, position, folders, done) {
  spawnClppJson("api", ["definition"], positionPayload(source, fileName, position), folders, done);
}

function apiReferences(source, fileName, position, folders, done) {
  spawnClppJson("api", ["references"], positionPayload(source, fileName, position), folders, done);
}

function apiSignature(source, fileName, position, folders, done) {
  spawnClppJson("api", ["signature"], positionPayload(source, fileName, position), folders, done);
}

function apiFormat(source, fileName, folders, done) {
  spawnClppJson("api", ["format"], { source, fileName: fileName || "untitled.clpp" }, folders, done);
}

function apiInlay(source, fileName, folders, done) {
  spawnClppJson(
    "api",
    ["inlay"],
    positionPayload(source, fileName, { line: 0, character: 0 }),
    folders,
    done
  );
}

function apiFolding(source, fileName, folders, done) {
  spawnClppJson(
    "api",
    ["folding"],
    positionPayload(source, fileName, { line: 0, character: 0 }),
    folders,
    done
  );
}

function apiHighlight(source, fileName, position, folders, done) {
  spawnClppJson("api", ["highlight"], positionPayload(source, fileName, position), folders, done);
}

function apiActions(source, fileName, position, folders, done) {
  spawnClppJson("api", ["actions"], positionPayload(source, fileName, position), folders, done);
}

function apiWorkspaceSymbols(source, fileName, folders, done) {
  spawnClppJson(
    "api",
    ["workspace-symbols"],
    positionPayload(source, fileName, { line: 0, character: 0 }),
    folders,
    done
  );
}

function apiCompile(source, fileName, folders, done) {
  spawnClppJson("api", ["compile"], { source, fileName: fileName || "untitled.clpp" }, folders, done);
}

module.exports = {
  findClpp,
  apiComplete,
  apiHover,
  apiSymbols,
  apiDefinition,
  apiReferences,
  apiSignature,
  apiFormat,
  apiInlay,
  apiFolding,
  apiHighlight,
  apiActions,
  apiWorkspaceSymbols,
  apiCompile,
  positionPayload,
};
