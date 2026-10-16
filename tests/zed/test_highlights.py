"""Regression for the reported Combat highlight and compiler semantic roles."""
from pathlib import Path
import argparse, os, re, subprocess, sys, tempfile
root = Path(__file__).resolve().parents[2]
grammar = root / 'tools/zed/grammar'
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', type=Path, default=root / 'build/ucrt64/src/clpp-vm.exe')
parser.add_argument('--tree-sitter', type=Path, default=grammar / ('node_modules/tree-sitter-cli/tree-sitter.exe' if os.name == 'nt' else 'node_modules/tree-sitter-cli/tree-sitter'))
options = parser.parse_args()
compiler = options.compiler.resolve()
cli = options.tree_sitter.resolve()
sources = [grammar / 'smoke.clp', *sorted((root / 'examples').rglob('*.clp')),
           *sorted((root / 'benchmarks').glob('*.clp'))]
env = dict(os.environ)
if os.name == 'nt' and 'CC' not in env:
    env['CC'] = 'C:/msys64/ucrt64/bin/gcc.exe'
result = subprocess.run([str(cli), 'parse', '-q', *map(str, sources)], cwd=grammar,
                        env=env, capture_output=True, text=True, encoding='utf8')
assert result.returncode == 0 and 'ERROR' not in result.stdout and 'MISSING' not in result.stdout, result.stdout + result.stderr
combat = root / 'examples/projects/combat/main.clp'
result = subprocess.run([str(cli), 'query', str(grammar.parent / 'languages/clpp/highlights.scm'), str(combat)],
                        cwd=grammar, env=env, capture_output=True, text=True, encoding='utf8')
assert result.returncode == 0, result.stderr
captures = {}
for kind, row, column, text in re.findall(r'capture: (?:\d+ - )?([\w.]+), start: \((\d+), (\d+)\), end: .*?text: `([^`]+)`', result.stdout):
    captures.setdefault((int(row), int(column), text), set()).add(kind)
# Locate names in the actual source, retaining exact line/column even if it moves.
lines = combat.read_text(encoding='utf8').splitlines()
decl = next(i for i, line in enumerate(lines) if line.startswith('optional<Action> DefaultBrain('))
for name, category, offset in [('DefaultBrain', 'function', 0), ('Fighter', 'type', 0),
                             ('battle', 'variable.parameter', 0), ('TeamA', 'property', 1),
                             ('Contains', 'function', 1), ('Action', 'type', 0)]:
    row = decl + offset
    col = lines[row].index(name)
    assert category in captures.get((row, col, name), set()), (name, category, captures.get((row, col, name)))
    if category in ('function', 'property'):
        assert 'type' not in captures[(row, col, name)], name + ' wrongly classified as type'
    print('PASS syntax role:', name, category)
for query in sorted((grammar.parent / 'languages/clpp').glob('*.scm')):
    subprocess.run([str(cli), 'query', '-q', str(query), str(sources[0])], cwd=grammar, env=env,
                   check=True, capture_output=True)
print(f'PASS {len(sources)} sources without parser errors; all queries valid')
result = subprocess.run([str(cli), 'query', str(grammar.parent / 'languages/clpp/highlights.scm'), str(sources[0])],
                        cwd=grammar, env=env, capture_output=True, text=True, encoding='utf8', check=True)
assert re.search(r'capture: .*variable.special.*text: `@this`', result.stdout), result.stdout
assert '#eq? @variable.special "self"' not in (grammar.parent / 'languages/clpp/highlights.scm').read_text()
print('PASS @this is the instance expression')
assert re.search(r'capture: .*comment.*text: `<< 5\) def com HERANÇA nominal\.`', result.stdout), result.stdout
assert not re.search(r'capture: .*keyword.*text: `def`.*HERANÇA', result.stdout)
print('PASS reported inheritance heading is a full line comment')
for keyword in ('switch', 'case', 'default', 'static_cast'):
    assert re.search(r'capture: .*keyword.*text: `' + keyword + '`', result.stdout), keyword
print('PASS switch branches and explicit casts are keywords')
smoke_captures = {}
for kind, row, column, text in re.findall(r'capture: (?:\d+ - )?([\w.]+), start: \((\d+), (\d+)\), end: .*?text: `([^`]+)`', result.stdout):
    smoke_captures.setdefault((int(row), int(column), text), set()).add(kind)
smoke_lines = sources[0].read_text(encoding='utf8').splitlines()
for prefix, word, role in [('auto storedFactory', 'Keep', 'function'),
                           ('decltype(', 'decltype', 'keyword'),
                           ('Vector3 position', 'Vector3', 'type.builtin'),
                           ('buffer bytes', 'buffer', 'type.builtin'),
                           ('auto typedOutcome', 'Outcome', 'type'),
                           ('post((attempts', 'attempts', 'variable'),
                           ('number pontuação', 'pontuação', 'variable')]:
    row = next(index for index, line in enumerate(smoke_lines) if line.startswith(prefix))
    column = re.search(r'\b' + re.escape(word) + r'\b', smoke_lines[row]).start()
    assert role in smoke_captures.get((row, column, word), set()), (word, role)
print('PASS stored generic function, decltype and builtin vector/buffer roles')
with tempfile.TemporaryDirectory(prefix='clpp-grammar-invalid-') as directory:
    invalid = Path(directory) / 'invalid.clp'
    for code in ('virtual def Item { number Value; }',
                 'def Item { public: number Value; }',
                 'def Invalid = 42;', 'def Invalid = true;', 'post(+1);',
                 'if (true) post(1);', '{ post(1); }'):
        invalid.write_text(code, encoding='utf8')
        rejected = subprocess.run([str(cli), 'parse', '-q', str(invalid)], cwd=grammar,
                                  env=env, capture_output=True, text=True, encoding='utf8', timeout=15)
        assert rejected.returncode != 0 and ('ERROR' in rejected.stdout or 'MISSING' in rejected.stdout), code
print('PASS unsupported syntax is not accepted by the editor grammar')
checked = subprocess.run([str(compiler), '--check', str(sources[0])],
                         cwd=root, env=env, capture_output=True, text=True, encoding='utf8', timeout=30)
assert checked.returncode == 0, checked.stdout + checked.stderr
print('PASS editor smoke fixture is also accepted by the real compiler')

sys.path.insert(0, str(root / 'tests/lsp'))
from lsp_client import Client
client = Client(str(compiler), root.as_posix())
try:
    uri = client.open(combat.as_posix(), combat.read_text(encoding='utf8'))
    response = client.req('textDocument/semanticTokens/full', {'textDocument': {'uri': uri}})
    legend = client.caps['semanticTokensProvider']['legend']['tokenTypes']
    data = response['result']['data']
    row = col = 0
    roles = {}
    for i in range(0, len(data), 5):
        dr, dc, length, category, modifiers = data[i:i+5]
        row += dr
        col = dc if dr else col + dc
        roles[(row, col)] = legend[category]
    for name, category, offset in [('DefaultBrain', 'function', 0), ('Fighter', 'struct', 0),
                                 ('battle', 'parameter', 0), ('TeamA', 'property', 1)]:
        row = decl + offset
        col = lines[row].index(name)
        assert roles.get((row, col)) == category, (name, roles.get((row, col)))
        print('PASS compiler role:', name, category)
finally:
    client.close()
