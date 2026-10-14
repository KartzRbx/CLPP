// Configure a separate presence LSP; never replace the CL++ language server.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const jsonc = require('../zed/grammar/node_modules/jsonc-parser');
const root = path.resolve(__dirname, '../..');
const options = {};
for (let i = 2; i < process.argv.length; i += 2) {
  if (process.argv[i] === '--enable') { options.enable = true; --i; continue; }
  if (!['--server', '--wasm'].includes(process.argv[i]) || !process.argv[i + 1]) {
    throw Error('Usage: node install-local.cjs --server executable --wasm extension.wasm [--enable]');
  }
  options[process.argv[i]] = path.resolve(process.argv[i + 1]);
}
for (const key of ['--server', '--wasm']) {
  if (!options[key] || !fs.statSync(options[key]).isFile()) throw Error(`Missing ${key}`);
}
const wasm = fs.readFileSync(options['--wasm']);
if (!wasm.subarray(0, 4).equals(Buffer.from([0, 97, 115, 109]))) throw Error('Invalid extension WebAssembly');
const settingsPath = path.join(process.env.APPDATA, 'Zed/settings.json');
let settings = fs.readFileSync(settingsPath, 'utf8');
const errors = [];
const current = jsonc.parse(settings, errors, { allowTrailingComma: true });
if (errors.length || !current) throw Error('Invalid existing Zed settings; nothing changed');
const destination = path.join(process.env.LOCALAPPDATA, 'Zed/extensions/installed/clpp-discord-presence');
const bytes = fs.readFileSync(options['--server']);
const hash = crypto.createHash('sha256').update(bytes).digest('hex');
const server = path.join(destination, 'bin', hash, 'discord-presence-lsp.exe');
fs.mkdirSync(path.dirname(server), { recursive: true });
if (!fs.existsSync(server)) fs.writeFileSync(server, bytes);
fs.copyFileSync(path.join(__dirname, 'zed/extension.toml'), path.join(destination, 'extension.toml'));
fs.writeFileSync(path.join(destination, 'extension.wasm'), wasm);
const update = (keys, value) => {
  settings = jsonc.applyEdits(settings, jsonc.modify(settings, keys, value,
    { formattingOptions: { insertSpaces: true, tabSize: 2, eol: '\n' } }));
};
update(['lsp', 'clpp_discord_presence', 'binary'], { path: server, arguments: [] });
update(['lsp', 'clpp_discord_presence', 'initialization_options'], {
  application_id: '1263505205522337886',
  details: 'Programando CL++', state: 'Projeto: {workspace} | {filename}',
  large_image: 'https://raw.githubusercontent.com/xhyrom/zed-discord-presence/main/assets/icons/zed.png',
  large_text: 'CL++', small_text: 'Zed', git_integration: false,
  idle: { timeout: 300, action: 'clear_activity' },
});
update(['languages', 'CL++', 'language_servers'], [...new Set([
  'clpp', ...(current.languages?.['CL++']?.language_servers || []).filter(id => !['clpp_discord_presence', '!clpp_discord_presence'].includes(id)),
  options.enable ? 'clpp_discord_presence' : '!clpp_discord_presence',
])]);
const backup = `${settingsPath}.presence-${Date.now()}.bak`;
fs.copyFileSync(settingsPath, backup, fs.constants.COPYFILE_EXCL);
fs.writeFileSync(settingsPath, settings);
console.log(`Installed CL++ Discord Presence (${options.enable ? 'enabled' : 'disabled'}). Settings backup: ${backup}`);
