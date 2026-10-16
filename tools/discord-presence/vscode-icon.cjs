// Local compatibility patch for iCrawl Discord Presence 5.9.2.
// Only the CL++ image mapping changes; transport, application ID and metadata stay upstream.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const ICON = 'https://raw.githubusercontent.com/KartzRbx/CLPP/main/assets/brand/clpp-256.png';
const ANCHOR = 'function resolveFileIcon(document) {\n';
const MARKER = '/* CLPP_ICON_MAPPING */';
const INSERTION = `  ${MARKER}\n  if (document.languageId === "clpp" || /\\.clp$/i.test(document.fileName)) return ${JSON.stringify(ICON)};\n`;
const digest = value => crypto.createHash('sha256').update(value).digest('hex');

function patchedBundle(source) {
  if (source.includes(MARKER)) {
    if (!source.includes(INSERTION)) throw Error('A different CL++ icon patch is present');
    return source;
  }
  if (source.split(ANCHOR).length !== 2) throw Error('Unsupported upstream bundle: icon function not unique');
  return source.replace(ANCHOR, ANCHOR + INSERTION);
}

function install(folder) {
  const manifest = JSON.parse(fs.readFileSync(path.join(folder, 'package.json'), 'utf8'));
  if (manifest.publisher !== 'icrawl' || manifest.name !== 'discord-vscode' || manifest.version !== '5.9.2') {
    throw Error('This local adapter supports icrawl.discord-vscode 5.9.2 only');
  }
  const target = path.join(folder, 'dist/extension.cjs');
  const original = fs.readFileSync(target, 'utf8');
  const updated = patchedBundle(original);
  if (original === updated) { console.log('CL++ Discord image mapping already installed'); return; }
  const backup = target + '.clpp-original';
  if (fs.existsSync(backup) && fs.readFileSync(backup, 'utf8') !== original) throw Error('Backup differs; review before replacing');
  if (!fs.existsSync(backup)) fs.copyFileSync(target, backup);
  const temporary = target + '.clpp-new';
  fs.writeFileSync(temporary, updated);
  fs.renameSync(temporary, target);
  fs.writeFileSync(target + '.clpp-icon.json', JSON.stringify({
    upstreamVersion: manifest.version, icon: ICON, originalSha256: digest(original), patchedSha256: digest(updated)
  }, null, 2) + '\n');
  console.log('Installed CL++ Discord image mapping; reload the VS Code window to apply');
}

module.exports = { patchedBundle, install, ICON, INSERTION };
if (require.main === module) {
  try {
    if (process.argv.length !== 3) throw Error('Usage: node vscode-icon.cjs <installed icrawl.discord-vscode-5.9.2 folder>');
    install(path.resolve(process.argv[2]));
  } catch (error) { console.error(error.message); process.exitCode = 1; }
}
