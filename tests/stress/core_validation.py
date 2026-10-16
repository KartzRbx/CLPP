"""Bounded CLI audit: current semantics, rejected legacy and malformed inputs.

Not a security proof. Each case runs in a separate process so crashes and hangs
are reported rather than aborting the rest of the matrix.
"""
import argparse
import json
import random
import subprocess
import tempfile
import time
from pathlib import Path


def cases():
    yield 'native-input-and-sleep-ranges', '''
link @clpp.input as Input; link @clpp.time as Time;
try { Input.Move(9223372036854775807, 0); } catch (error) { post("input rejected"); }
try { Time.Sleep(1e300); } catch (error) { post("sleep rejected"); }
''', 'input rejected\nsleep rejected\n'
    yield 'optional-composite-and-exact-metadata', '''
const number limit = 9223372036854775807;
optional<array<number>> items = array<number>(7);
post(limit); if (items != null) { post(items[0]); }
''', '9223372036854775807\n7\n'
    yield 'audio-handle-range', '''
link @clpp.audio as Audio;
try { Audio.Play(9223372036854775807, 1, 0, 1, false); }
catch (error) { post("rejected"); }
Audio.Stop(-9223372036854775808);
post(Audio.IsPlaying(-9223372036854775808));
''', 'rejected\nfalse\n'
    yield 'generic-capture', '''
def Getter<T> = () -> T;
template <typename T> Getter<T> Keep(T value) {
  return func() -> T { return value; };
}
Getter<number> read = Keep<number>(42);
post(read());
''', '42\n'
    yield 'switch-cast', '''
number sum = 0;
for (number i = 0; i < 5; i += 1) {
  switch (i) { case 2: continue; default: sum += i; }
}
post(static_cast<number>(static_cast<float>(sum)));
''', '8\n'
    yield 'event-cleanup', '''
signal<number> changed;
number total = 0;
changed~>Connect(func(number value) -> void { total += value; });
changed::Fire(3);
changed::Clear();
changed::Fire(9);
post(total);
''', '3\n'
    yield 'shadowing-and-sibling-scopes', '''
number value = 1;
if (true) { number value = 2; post(value); }
if (true) { number value = 3; post(value); }
for (number value = 0; value < 1; value += 1) { post(value); }
post(value);
''', '2\n3\n0\n1\n'
    yield 'wide-distinct-bindings', ''.join(f'number value{i} = {i};\n' for i in range(1000)) + 'post(value999);\n', '999\n'
    for name, source in {
        'struct': 'struct Item { number Value; }',
        'named-func': 'func Read() { return 1; }',
        'plain-enum': 'enum State { Idle, Running }',
        'type-alias': 'type State = "Idle" | "Running";',
        'using': 'using Item = number;',
        'namespace': 'namespace Example { number Value = 1; }',
        'new': 'def Item { number Value; } auto value = new Item(1);',
        'positional-data': 'def Item { number Value; } auto value = Item(1);',
        'receiver': 'def Item { number Value; number Read() { return self.Value; } }',
        'default-argument': 'number Read(number value = 1) { return value; }',
        'untyped-signal': 'signal changed;',
        'untyped-task': 'task pending;',
        'legacy-match-arm': 'match (1) { 1 ~> post(1); }',
        'wrong-generic-result': 'def Getter<T> = () -> T; Getter<number> read = func() -> string { return "bad"; };',
        'wrong-optional': 'optional<number> item = "bad";',
        'duplicate-default': 'switch (1) { default: post(1); default: post(2); }',
        'duplicate-local': 'number value = 1; number value = 2;',
        'duplicate-inner-local': 'if (true) { number value = 1; number value = 2; }',
        'unclosed-string': 'post("unterminated);',
        'unclosed-comment': "<<[ unterminated",
        'unclosed-block': 'if (true) { post(1);',
        'invalid-byte': '\x00\x01\x02',
        'integer-out-of-range': 'post(9223372036854775808);',
        'broken-hex': 'post(0xGG);',
        'broken-exponent': 'post(1e+);',
        'invalid-generic-delimiters': 'array<array<number> broken = {};',
    }.items():
        yield name, source, None
    # Exercise balanced and truncated nesting without an unbounded generator.
    for depth in (8, 32, 64):
        yield f'nesting-{depth}', 'post(' + '(' * depth + '1' + ')' * depth + ');', '1\n'
        yield f'truncated-{depth}', 'post(' + '(' * depth + '1;', None
    for depth in (128, 256, 512):
        yield f'bounded-parentheses-{depth}', 'post(' + '(' * depth + '1' + ')' * depth + ');', None
        yield f'bounded-unary-{depth}', 'post(' + '!' * depth + 'true);', None
        yield f'bounded-blocks-{depth}', 'if (true) {' * depth + 'post(1);' + '}' * depth, None
    yield 'bounded-operator-chain', 'post(' + '+'.join(['1'] * 512) + ');', None
    yield 'bounded-member-chain', 'post(value' + '.Field' * 512 + ');', None
    yield 'operator-chain-128', 'post(' + '+'.join(['1'] * 128) + ');', '128\n'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--fuzz', type=int, default=0)
    parser.add_argument('--seed', type=int, default=20261013)
    args = parser.parse_args()
    if args.fuzz < 0 or args.fuzz > 10000:
        parser.error('--fuzz must be between zero and 10000')
    compiler = args.compiler.resolve()
    results = []
    matrix = list(cases())
    rng = random.Random(args.seed)
    alphabet = '(){}[]<>+-*/;?:"abcdef012345 \n\t@!&|=é'
    seeds = [code for _, code, expected in matrix if expected is not None]
    seeds.extend(['def Item { number Value; } auto item = Item { Value = 7 }; post(item.Value);',
                  'def Apply<T> = (T)->T; def Box<T> { Apply<T> Run; }',
                  '<<[ comment ]>> number café = 7; post(café);',
                  'dictionary<string,number> values = dictionary<string,number>(); post(values["missing"]);'])
    for index in range(args.fuzz):
        # Mutate real programs so the parser/binder/checker are exercised. Check-only
        # requests cannot execute native effects even if a mutation remains valid.
        mutated = rng.choice(seeds)
        for _ in range(rng.randint(1, 5)):
            start = rng.randrange(len(mutated) + 1)
            end = min(len(mutated), start + rng.randint(0, 20))
            inserted = ''.join(rng.choice(alphabet) for _ in range(rng.randint(0, 20)))
            mutated = mutated[:start] + inserted + mutated[end:]
        matrix.append((f'mutated-seed-{args.seed}-{index}', mutated, None))
    with tempfile.TemporaryDirectory(prefix='clpp-core-audit-') as directory:
        source = Path(directory) / 'audit.clp'
        for name, code, expected in matrix:
            source.write_text(code, encoding='utf8')
            started = time.perf_counter()
            try:
                check_only = name.startswith('mutated-seed-')
                command = [str(compiler), '--check', str(source)] if check_only else [str(compiler), str(source)]
                result = subprocess.run(command, capture_output=True,
                                        text=True, encoding='utf8', timeout=15)
                # Normal diagnostic failure is distinct from an OS crash code.
                if check_only:
                    passed = result.returncode == 0 or (result.returncode == 1 and bool(result.stderr.strip()))
                else:
                    passed = (result.returncode == 0 and result.stdout == expected) if expected is not None else (
                        result.returncode == 1 and bool(result.stderr.strip()))
                item = dict(name=name, passed=passed, returncode=result.returncode,
                            stdout=result.stdout, stderr=result.stderr)
            except subprocess.TimeoutExpired:
                item = dict(name=name, passed=False, error='timeout')
            if not item['passed']:
                item['source'] = code
            item['elapsed_ms'] = round((time.perf_counter() - started) * 1000, 3)
            results.append(item)
            print(('PASS' if item['passed'] else 'FAIL'), name, flush=True)
    if args.report:
        args.report.write_text(json.dumps(results, indent=2), encoding='utf8')
    failed = [item for item in results if not item['passed']]
    print(f'{len(results)} cases; {len(failed)} failures')
    for item in failed:
        print(json.dumps(item, ensure_ascii=True))
    return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
