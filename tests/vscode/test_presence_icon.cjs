const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const { patchedBundle, install, ICON } = require('../../tools/discord-presence/vscode-icon.cjs');

const original = 'function resolveFileIcon(document) {\n  return "text";\n}\n';
const updated = patchedBundle(original);
const context = {}; vm.createContext(context); vm.runInContext(updated, context);
assert.equal(context.resolveFileIcon({ languageId: 'clpp', fileName: '/private/folder/test.clp' }), ICON);
assert.equal(context.resolveFileIcon({ languageId: 'plaintext', fileName: 'TEST.CLP' }), ICON);
assert.equal(context.resolveFileIcon({ languageId: 'javascript', fileName: 'app.js' }), 'text');
assert.equal(patchedBundle(updated), updated);
assert.throws(() => patchedBundle('function anotherIcon() {}'), /Unsupported/);
const folder = fs.mkdtempSync(path.join(os.tmpdir(), 'clpp-icon-test-'));
try {
  fs.mkdirSync(path.join(folder, 'dist'));
  fs.writeFileSync(path.join(folder, 'package.json'), JSON.stringify({publisher:'icrawl',name:'discord-vscode',version:'5.9.2'}));
  const file = path.join(folder, 'dist/extension.cjs'); fs.writeFileSync(file, original);
  install(folder); install(folder);
  assert.equal(fs.readFileSync(file + '.clpp-original', 'utf8'), original);
  assert.equal(fs.readFileSync(file, 'utf8'), updated);
  const info = JSON.parse(fs.readFileSync(file + '.clpp-icon.json', 'utf8'));
  assert.notEqual(info.originalSha256, info.patchedSha256);
} finally {
  assert.ok(path.resolve(folder).startsWith(path.resolve(os.tmpdir()) + path.sep));
  assert.ok(path.basename(folder).startsWith('clpp-icon-test-'));
  fs.rmSync(folder, {recursive:true,force:true});
}
console.log('PASS CL++ mapping, unrelated languages, backup, guard and idempotence');
