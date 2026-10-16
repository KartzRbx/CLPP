const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {test} = require('node:test');

function discover(platform, existing, configured) {
  const context = {
    module: {exports: {}}, exports: {},
    process: {platform, arch: 'x64', env: {PATH: '', LOCALAPPDATA: 'C:/Users/test/AppData/Local'}},
    require(name) {
      if (name === 'vscode') return {workspace: {getConfiguration: () => ({get: () => configured})}};
      if (name === 'fs') return {existsSync: file => existing.includes(file)};
      if (name === 'os') return {homedir: () => '/home/test'};
      return require(name);
    },
  };
  const source = fs.readFileSync(path.join(__dirname, '../../tools/vscode/extension.js'), 'utf8');
  vm.runInNewContext(source + '\nthis.findInstalledServer = findServer;', context);
  return context.findInstalledServer({asAbsolutePath: () => '/missing/bundled/compiler'});
}

test('regression: Windows installer bin layout is discovered', () => {
  const executable = path.join('C:/Users/test/AppData/Local', 'Programs', 'CLPP-VM', 'bin', 'clpp-vm.exe');
  const actual = discover('win32', [executable]);
  assert.equal(actual.path, executable);
  assert.equal(actual.source, 'installer');
});

test('regression: configured compiler takes precedence over installer', () => {
  const configured = path.join('C:/tools', 'custom-compiler.exe');
  const actual = discover('win32', [configured], configured);
  assert.equal(actual.path, configured);
});

test('regression: POSIX native VM install is discovered', () => {
  const executable = path.join('/home/test', '.local', 'bin', 'clpp-vm');
  const actual = discover('linux', [executable]);
  assert.equal(actual.path, executable);
});
