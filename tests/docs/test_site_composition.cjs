const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

const compose = process.env.CLPP_COMPOSE_SCRIPT || path.resolve(__dirname, '../../scripts/compose-luau-docs.mjs');
test('independent documentation keeps its namespace and legacy API redirects', { skip: !fs.existsSync(compose) }, () => {
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'clpp-site-compose-'));
  const source = path.join(temporary, 'source');
  const destination = path.join(temporary, 'site');
  const write = (base, file, content) => {
    const full = path.join(base, file);
    fs.mkdirSync(path.dirname(full), { recursive: true });
    fs.writeFileSync(full, content);
  };
  const original = '<a href="/CLPP/api/Builtins">API</a><p>Historical release prose: não.</p>';
  try {
    write(source, 'index.html', 'Luau home');
    write(source, 'docs/io/index.html', original);
    write(source, 'docs/shared/index.html', 'Luau shared page');
    write(source, 'api/Builtins/index.html', 'Luau API');
    write(destination, 'index.html', 'VM home');
    write(destination, 'docs/shared/index.html', 'VM page');
    for (let pass = 0; pass < 2; pass++) {
      execFileSync(process.execPath, [compose, source, destination], { windowsHide: true });
      assert.equal(fs.readFileSync(path.join(destination, 'index.html'), 'utf8'), 'VM home');
      assert.equal(fs.readFileSync(path.join(destination, 'docs/shared/index.html'), 'utf8'), 'VM page');
      assert.equal(fs.readFileSync(path.join(source, 'docs/io/index.html'), 'utf8'), original);
      const copied = fs.readFileSync(path.join(destination, 'luau/docs/io/index.html'), 'utf8');
      assert.ok(copied.includes('href="/CLPP/luau/api/Builtins"'));
      assert.ok(copied.includes('não'));
      assert.ok(fs.readFileSync(path.join(destination, 'api/Builtins/index.html'), 'utf8').includes('/CLPP/luau/api/Builtins/'));
      assert.ok(fs.readFileSync(path.join(destination, 'docs/io/index.html'), 'utf8').includes('/CLPP/luau/docs/io/'));
    }
  } finally {
    assert.ok(path.resolve(temporary).startsWith(path.resolve(os.tmpdir()) + path.sep));
    assert.ok(path.basename(temporary).startsWith('clpp-site-compose-'));
    fs.rmSync(temporary, { recursive: true, force: true });
  }
});
