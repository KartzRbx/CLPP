"""Measure --check wall time independently of executing a CL++ program.

Includes process startup. Use a known build configuration and an idle machine
before interpreting timings as a performance comparison.
"""
import argparse
import json
import statistics
import subprocess
import tempfile
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--runs', type=int, default=3)
    parser.add_argument('--sizes', type=int, nargs='+', default=[100, 1000, 10000])
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.runs < 1 or any(size < 1 for size in args.sizes):
        parser.error('runs and sizes must be positive')
    compiler = args.compiler.resolve()
    rows = []
    with tempfile.TemporaryDirectory(prefix='clpp-check-scaling-') as directory:
        source = Path(directory) / 'scaling.clp'
        for size in args.sizes:
            code = ''.join(f'const auto value{i} = {i};\n' for i in range(size))
            code += f'post(value{size-1});\n'
            source.write_text(code, encoding='utf8')
            elapsed = []
            for _ in range(args.runs):
                started = time.perf_counter()
                result = subprocess.run([str(compiler), '--check', str(source)],
                                        capture_output=True, text=True, timeout=120)
                if result.returncode or result.stdout:
                    raise RuntimeError(f'check failed or executed code: {result.stderr} {result.stdout}')
                elapsed.append((time.perf_counter() - started) * 1000)
            row = dict(bindings=size, source_bytes=len(code.encode('utf8')),
                       median_ms=statistics.median(elapsed), samples_ms=elapsed)
            rows.append(row)
            print(json.dumps(row), flush=True)
    if args.report:
        args.report.write_text(json.dumps(rows, indent=2), encoding='utf8')


if __name__ == '__main__':
    main()
