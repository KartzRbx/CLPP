"use strict";

const fs = require("fs");
const path = require("path");
const assert = require("assert");
const { loadEngine, lintDocument } = require("./intellisense");

const data = JSON.parse(fs.readFileSync(path.join(__dirname, "data", "completions.json"), "utf8"));
const playerData = fs.readFileSync(
  path.join(__dirname, "..", "..", "examples", "shared", "PlayerData.clh"),
  "utf8"
);
const engine = loadEngine(data);

function labels(members) {
  return members.map((m) => m.label).sort();
}

const source = `
#include "../shared/PlayerData.clh"

void LeaderstatsServer::PlayerEntered(Player* player) {
    Players* players = GetService<Players>();
    PlayerData Paths = DataService.Paths;
    Data* playerData = DataService.Server.WaitFor(player);
    Paths.
    playerData.GetChangedSignal(Paths.Currencies.
}
`;

const symbols = engine.indexDocument(source, [playerData]);

assert.strictEqual(symbols.vars.get("Paths").type, "PlayerData");
assert.ok(symbols.types.PlayerData, "parses PlayerData from the header");
assert.deepStrictEqual(labels(symbols.types.PlayerData.properties), ["Currencies", "Inventory"]);

const pathsDot = engine.resolve("    Paths.", symbols);
assert.deepStrictEqual(labels(pathsDot.members), ["Currencies", "Inventory"]);
assert.ok(!labels(pathsDot.members).includes("Archivable"), "does not fall back to Instance");

const currenciesDot = engine.resolve("    Paths.Currencies.", symbols);
assert.deepStrictEqual(labels(currenciesDot.members), ["Coins", "Rebirths"]);

const coinsInCall = engine.resolve("    playerData.GetChangedSignal(Paths.Currencies.", symbols);
assert.deepStrictEqual(labels(coinsInCall.members), ["Coins", "Rebirths"]);

const modulePaths = engine.resolve("    DataService.Paths.", symbols);
assert.deepStrictEqual(labels(modulePaths.members), ["Currencies", "Inventory"]);

const colonModulePaths = engine.resolve("    DataService:Paths.", symbols);
assert.deepStrictEqual(labels(colonModulePaths.members), ["Currencies", "Inventory"]);

const serverPaths = engine.resolve("    DataService.Server.Paths.", symbols);
assert.deepStrictEqual(labels(serverPaths.members), ["Currencies", "Inventory"]);

const dataMethods = engine.resolve("    playerData.", symbols);
assert.ok(labels(dataMethods.members).includes("GetChangedSignal"));

const staticConnect = engine.resolve("    players.PlayerAdded::", symbols);
assert.ok(labels(staticConnect.members).includes("Connect"));

const protectedCall = engine.resolve("    player:", symbols);
assert.ok(
  labels(protectedCall.members).includes("Kick")
    || labels(protectedCall.members).includes("FindFirstChild"),
  labels(protectedCall.members).join(",")
);

const lint = lintDocument(`void F() {
    const int = 1;
    int n = "x"
    error("no");
}
`);
assert.ok(lint.some((d) => d.message.includes("expected a name")));
assert.ok(lint.some((d) => d.message.includes("cannot initialize")));
assert.ok(lint.some((d) => d.message.includes("missing ';'")));
assert.ok(lint.some((d) => d.message.includes("report")));

const convertLint = lintDocument(`void F() { string s = tostring(1); float n = tonumber("2"); }`);
assert.ok(convertLint.some((d) => d.message.includes("to_string")));
assert.ok(convertLint.some((d) => d.message.includes("to_number")));

const captureLint = lintDocument(`void F() {
    OnChange(func [](int x) {
    });
    []() {
    }
}
`);
assert.ok(captureLint.some((d) => d.message.includes("func (params)")));
assert.ok(captureLint.filter((d) => d.message.includes("func (params)")).length >= 2);

const atOutside = lintDocument(`void init() {
    @this;
    @janitor.Cleanup();
}
`);
assert.ok(atOutside.some((d) => d.message.includes("@this") && d.message.includes("Class::Method")));
assert.ok(atOutside.some((d) => d.message.includes("@janitor")));

const atInside = lintDocument(`void CombatServer::BindPart(BasePart* part) {
    @janitor.Add(part, "Destroy");
    other.Register(@this);
}
`);
assert.ok(!atInside.some((d) => d.message.includes("Class::Method")));

console.log("intellisense.test.js ok");
