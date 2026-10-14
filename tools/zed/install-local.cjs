// Install the prebuilt extension and merge only CL++ settings, preserving JSONC.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const jsonc = require('./grammar/node_modules/jsonc-parser');
const root = path.resolve(__dirname, '../..');
const options = {};
for (let i = 2; i < process.argv.length; i += 2) {
  const key = process.argv[i];
  if (!['--package', '--compiler'].includes(key) || !process.argv[i + 1]) {
    throw new Error('Usage: node install-local.cjs [--package directory] [--compiler executable]');
  }
  options[key] = path.resolve(process.argv[i + 1]);
}
const source = options['--package'] || path.join(root, 'build/zed-extension/clpp');
const destination = path.join(process.env.LOCALAPPDATA, 'Zed/extensions/installed/clpp');
const settingsPath = path.join(process.env.APPDATA, 'Zed/settings.json');
const executable = options['--compiler'] || path.join(root, 'build/ucrt64/src/clpp-vm.exe');
for (const required of ['extension.toml', 'extension.wasm', 'grammars/clpp.wasm']) {
  if (!fs.existsSync(path.join(source, required))) throw new Error(`Missing ${required}; run build.ps1 first`);
}
if (!fs.existsSync(executable)) throw new Error('Build the CL++ compiler first');
// Run an immutable snapshot: rebuilding the checkout must not require stopping
// the editor's live server, nor silently change its executable underneath it.
const compilerBytes = fs.readFileSync(executable);
const compilerHash = crypto.createHash('sha256').update(compilerBytes).digest('hex');
const installedExecutable = path.join(destination, 'bin', compilerHash, path.basename(executable));
let settings = fs.readFileSync(settingsPath, 'utf8');
const errors = [];
const currentSettings = jsonc.parse(settings, errors, { allowTrailingComma: true });
if (errors.length) throw new Error('Existing Zed settings contain invalid JSONC; settings were not changed');
const backup = `${settingsPath}.clpp-${Date.now()}.bak`;
fs.copyFileSync(settingsPath, backup, fs.constants.COPYFILE_EXCL);
const update = (keys, value) => {
  settings = jsonc.applyEdits(settings, jsonc.modify(settings, keys, value,
    { formattingOptions: { insertSpaces: true, tabSize: 2, eol: '\n' } }));
};
update(['lsp', 'clpp', 'binary', 'path'], installedExecutable);
update(['lsp', 'clpp', 'binary', 'arguments'], ['--lsp']);
update(['lsp', 'clpp', 'binary', 'env', 'PATH'], `C:\\msys64\\ucrt64\\bin;${process.env.PATH}`);
update(['languages', 'CL++', 'tab_size'], 2);
update(['languages', 'CL++', 'language_servers'], [...new Set([
  'clpp', ...(currentSettings.languages?.['CL++']?.language_servers || []).filter(id => id !== '!clpp'),
])]);
update(['languages', 'CL++', 'lsp_document_colors'], 'inlay');
update(['languages', 'CL++', 'semantic_tokens'], 'combined');
fs.mkdirSync(destination, { recursive: true });
fs.cpSync(source, destination, { recursive: true });
fs.mkdirSync(path.dirname(installedExecutable), { recursive: true });
if (!fs.existsSync(installedExecutable)) fs.writeFileSync(installedExecutable, compilerBytes);
// Extend the user's chosen icon theme locally rather than changing its extension.
const installed = path.dirname(destination);
const indexPath = path.join(path.dirname(installed), 'index.json');
const index = JSON.parse(fs.readFileSync(indexPath, 'utf8'));
const selected = currentSettings.icon_theme?.replace(/ \+ CL\+\+$/, '');
if (selected && selected !== 'CL++ Icons') {
  let found = false;
  for (const [id, entry] of Object.entries(index.extensions)) {
    for (const relative of entry.manifest.icon_themes || []) {
      const base = path.join(installed, id);
      const data = JSON.parse(fs.readFileSync(path.join(base, relative), 'utf8'));
      const theme = data.themes.find(t => t.name === selected);
      if (!theme) continue;
      const clone = JSON.parse(JSON.stringify(theme));
      const rewrite = value => {
        if (!value || typeof value !== 'object') return;
        for (const [key, child] of Object.entries(value)) {
          if (typeof child === 'string' && (key === 'path' || key === 'collapsed' || key === 'expanded')) {
            const original = path.resolve(base, child);
            if (!original.startsWith(base + path.sep)) throw new Error('Icon asset outside its extension');
            const target = path.join(destination, 'icons/base', path.relative(base, original));
            fs.mkdirSync(path.dirname(target), { recursive: true });
            fs.copyFileSync(original, target);
            value[key] = './' + path.relative(destination, target).replaceAll('\\', '/');
          } else rewrite(child);
        }
      };
      rewrite(clone);
      clone.name = selected + ' + CL++';
      clone.file_suffixes = { ...clone.file_suffixes, clp: 'clpp' };
      clone.file_icons = { ...clone.file_icons, clpp: { path: './icons/clpp.svg' } };
      fs.writeFileSync(path.join(destination, 'icon_themes/clpp.json'), JSON.stringify({
        $schema: 'https://zed.dev/schema/icon_themes/v0.3.0.json', name: clone.name,
        author: data.author, themes: [clone],
      }, null, 2));
      update(['icon_theme'], clone.name);
      found = true;
      break;
    }
    if (found) break;
  }
  if (!found) update(['icon_theme'], 'CL++ Icons');
} else if (!selected) update(['icon_theme'], 'CL++ Icons');
fs.writeFileSync(settingsPath, settings);
console.log(`Installed: ${destination}\nSettings backup: ${backup}\nRestart Zed or reload extensions, then open a .clp file.`);

