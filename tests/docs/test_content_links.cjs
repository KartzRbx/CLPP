const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {execFileSync} = require('node:child_process');

test('regression: external Markdown source links retain their URL and fragment', () => {
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'clpp-content-links-'));
  try {
    for (const dir of ['www', 'docs/guide', 'docs/handbook', 'docs/reference']) {
      fs.mkdirSync(path.join(temporary, dir), {recursive:true});
    }
    const source = path.resolve(__dirname, '../../www/build-content.mjs');
    fs.copyFileSync(source, path.join(temporary, 'www/build-content.mjs'));
    const external = 'https://github.com/KartzRbx/CLPP/blob/revision/docs/guide/07-structs-and-inheritance.md#fields';
    const protocolRelative = '//example.com/guide.md';
    fs.writeFileSync(path.join(temporary, 'docs/guide/00-test.md'),
      `# Test\n[Candidate](${external})\n[External](${protocolRelative})\n[Local](../reference/status.md#stable)\n`);
    fs.writeFileSync(path.join(temporary, 'docs/reference/status.md'), '# Status\n');
    execFileSync(process.execPath, [path.join(temporary, 'www/build-content.mjs')]);
    const result = fs.readFileSync(path.join(temporary, 'www/src/content/docs/guide/00-test.md'), 'utf8');
    assert.ok(result.includes(`](${external})`), 'external GitHub source URL must remain intact');
    assert.ok(result.includes(`](${protocolRelative})`), 'protocol-relative URL must remain intact');
    assert.ok(result.includes('](/CLPP/reference/status/#stable)'), 'local links still become site routes');
  } finally {
    assert.ok(path.resolve(temporary).startsWith(path.resolve(os.tmpdir()) + path.sep));
    assert.ok(path.basename(temporary).startsWith('clpp-content-links-'));
    fs.rmSync(temporary, {recursive:true, force:true});
  }
});
