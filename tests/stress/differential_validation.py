"""Compare deterministic generated programs with independent Python oracles."""
import argparse
from pathlib import Path
import random
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET


def scenarios(seed):
    rng = random.Random(seed)

    def expression(depth):
        if not depth:
            value = rng.randint(-9, 9)
            return f'({value})', value
        left, a = expression(depth - 1)
        right, b = expression(depth - 1)
        operation = rng.choice(['+', '-', '*'])
        value = a + b if operation == '+' else a - b if operation == '-' else a * b
        return f'({left} {operation} {right})', value

    code, expected = expression(4)
    yield 'expression', f'post({code});', f'{expected}\n'
    count, scale, offset = rng.randint(1, 45), rng.randint(-7, 7), rng.randint(-20, 20)
    code = (f'number total = {offset};\n'
            f'for (number i = 0; i < {count}; i += 1) {{\n'
            f'  if (i % 3 == 0) {{ total += i * {scale}; }} else {{ total -= i; }}\n'
            '}\npost(total);')
    expected = offset + sum(i * scale if i % 3 == 0 else -i for i in range(count))
    yield 'procedural-loop', code, f'{expected}\n'
    start, first, second = [rng.randint(-99, 99) for _ in range(3)]
    code = ('def Transform<T> = (T)->T;\n'
            'template <typename T> Transform<T> Accumulate(T value) where T: number {\n'
            '  return func(T change)->T { value += change; return value; };\n'
            '}\n'
            f'auto accumulate = Accumulate<number>({start});\n'
            f'post(accumulate({first})); post(accumulate({second}));')
    yield 'generic-reference-closure', code, f'{start + first}\n{start + first + second}\n'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--seeds', type=int, default=12)
    parser.add_argument('--junit', type=Path)
    args = parser.parse_args()
    if not 1 <= args.seeds <= 1000:
        parser.error('--seeds must be 1..1000')
    compilers = [args.compiler.resolve()]
    if args.baseline:
        compilers.append(args.baseline.resolve())
    suite = ET.Element('testsuite', name='differential-language-semantics')
    failures = 0
    with tempfile.TemporaryDirectory(prefix='clpp-differential-') as temporary:
        source = Path(temporary) / 'oracle.clp'
        for seed in range(args.seeds):
            for name, code, expected in scenarios(20261016 + seed):
                source.write_text(code, encoding='utf8')
                started, error = time.monotonic(), ''
                try:
                    for compiler in compilers:
                        result = subprocess.run([str(compiler), str(source)], capture_output=True,
                                                text=True, encoding='utf8', timeout=15)
                        assert result.returncode == 0, f'{compiler}: {result.returncode}\n{result.stderr}'
                        assert result.stdout == expected, f'{compiler}: expected {expected!r}, got {result.stdout!r}'
                except (AssertionError, subprocess.TimeoutExpired) as failure:
                    failures += 1
                    error = str(failure) + '\n' + code
                case = ET.SubElement(suite, 'testcase', name=f'{name}-seed-{20261016 + seed}', time=str(time.monotonic() - started))
                if error:
                    ET.SubElement(case, 'failure', message=error[:400]).text = error
                    print('FAIL', name, seed, error, flush=True)
    total = args.seeds * 3
    suite.set('tests', str(total))
    suite.set('failures', str(failures))
    if args.junit:
        ET.ElementTree(suite).write(args.junit, encoding='utf8', xml_declaration=True)
    print(f'{total} independent oracle scenarios; {len(compilers)} compiler builds; {failures} failures')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
