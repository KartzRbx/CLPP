const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const cp = require('node:child_process');
const { test } = require('node:test');
const jsonc = require('../../tools/zed/grammar/node_modules/jsonc-parser');
const root = path.resolve(__dirname, '../..');

test('regression: editor compiler snapshot survives rebuild and preserves presence server', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'clpp-installer-regression-'));
  try {
    const local = path.join(dir, 'local'), roaming = path.join(dir, 'roaming');
    fs.mkdirSync(path.join(roaming, 'Zed'), { recursive: true });
    fs.mkdirSync(path.join(local, 'Zed/extensions'), { recursive: true });
    fs.writeFileSync(path.join(local, 'Zed/extensions/index.json'), JSON.stringify({ extensions: {} }));
    const settings = path.join(roaming, 'Zed/settings.json');
    fs.writeFileSync(settings, '// preserve this comment\n' + JSON.stringify({
      theme: 'My theme', languages: { 'CL++': { language_servers: ['clpp', 'clpp_discord_presence'] } },
      lsp: { clpp: { binary: { env: { PATH: 'user-selected-path', CUSTOM: 'preserved' } } } },
    }));
    const pkg = path.join(dir, 'package');
    fs.mkdirSync(path.join(pkg, 'grammars'), { recursive: true });
    for (const name of ['extension.toml', 'extension.wasm', 'grammars/clpp.wasm']) fs.writeFileSync(path.join(pkg, name), 'fixture');
    const compiler = path.join(dir, 'clpp-vm.exe');
    fs.writeFileSync(compiler, 'compiler version one');
    const env = { ...process.env, APPDATA: roaming, LOCALAPPDATA: local };
    cp.execFileSync(process.execPath, [path.join(root, 'tools/zed/install-local.cjs'), '--package', pkg, '--compiler', compiler], { env });
    let data = jsonc.parse(fs.readFileSync(settings, 'utf8'));
    const first = data.lsp.clpp.binary.path;
    assert.notEqual(first, compiler);
    assert.equal(fs.readFileSync(first, 'utf8'), 'compiler version one');
    fs.writeFileSync(compiler, 'compiler version two');
    assert.equal(fs.readFileSync(first, 'utf8'), 'compiler version one');
    cp.execFileSync(process.execPath, [path.join(root, 'tools/zed/install-local.cjs'), '--package', pkg, '--compiler', compiler], { env });
    data = jsonc.parse(fs.readFileSync(settings, 'utf8'));
    assert.notEqual(data.lsp.clpp.binary.path, first);
    assert.equal(fs.readFileSync(data.lsp.clpp.binary.path, 'utf8'), 'compiler version two');
    assert.deepEqual(data.languages['CL++'].language_servers, ['clpp', 'clpp_discord_presence']);
    assert.equal(data.theme, 'My theme');
    assert.deepEqual(data.lsp.clpp.binary.env, { PATH: 'user-selected-path', CUSTOM: 'preserved' });
    assert.ok(fs.readFileSync(settings, 'utf8').startsWith('// preserve this comment'));
    assert.ok(fs.readdirSync(path.dirname(settings)).some(name => name.endsWith('.bak')));
  } finally {
    assert.ok(dir.startsWith(path.join(os.tmpdir(), 'clpp-installer-regression-')));
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

test('regression: presence installation preserves compiler and user settings', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'clpp-presence-regression-'));
  try {
    const local = path.join(dir, 'local'), roaming = path.join(dir, 'roaming');
    fs.mkdirSync(path.join(roaming, 'Zed'), { recursive: true });
    const settings = path.join(roaming, 'Zed/settings.json');
    fs.writeFileSync(settings, '// untouched theme\n' + JSON.stringify({
      theme: 'My theme', lsp: { clpp: { binary: { path: 'existing-compiler' } } },
      languages: { 'CL++': { language_servers: ['clpp', 'another-server'] } },
    }));
    const server = path.join(dir, 'presence.exe'), wasm = path.join(dir, 'presence.wasm');
    fs.writeFileSync(server, 'presence fixture'); fs.writeFileSync(wasm, Buffer.from([0, 97, 115, 109]));
    const env = { ...process.env, APPDATA: roaming, LOCALAPPDATA: local };
    cp.execFileSync(process.execPath, [path.join(root, 'tools/discord-presence/install-local.cjs'), '--server', server, '--wasm', wasm], { env });
    const data = jsonc.parse(fs.readFileSync(settings, 'utf8'));
    assert.equal(data.lsp.clpp.binary.path, 'existing-compiler');
    assert.equal(data.theme, 'My theme');
    assert.deepEqual(data.languages['CL++'].language_servers, ['clpp', 'another-server', '!clpp_discord_presence']);
    assert.equal(data.lsp.clpp_discord_presence.initialization_options.details, 'Programando CL++');
    assert.equal(fs.readFileSync(data.lsp.clpp_discord_presence.binary.path, 'utf8'), 'presence fixture');
    assert.ok(fs.readFileSync(settings, 'utf8').startsWith('// untouched theme'));
  } finally {
    assert.ok(dir.startsWith(path.join(os.tmpdir(), 'clpp-presence-regression-')));
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
