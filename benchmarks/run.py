"""CL++ benchmarks: median wall time of N runs per program, next to CPython.

Usage: python benchmarks/run.py <clpp> [--baseline <old clpp>] [--runs 5] [--markdown]
Each benchmark exists as <name>.clp and <name>.py with the same work and the same output.
"""
import argparse
import os
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
NAMES = ["fib", "loop", "structs", "vectors", "strings"]


def measure(command, runs):
    times, output = [], None
    for _ in range(runs):
        start = time.perf_counter()
        result = subprocess.run(command, capture_output=True, text=True, timeout=300)
        times.append(time.perf_counter() - start)
        output = result.stdout.strip()
    return statistics.median(times), output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("clpp")
    parser.add_argument("--baseline")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--markdown", action="store_true")
    args = parser.parse_args()
    rows = []
    for name in NAMES:
        clp = os.path.join(HERE, name + ".clp")
        py = os.path.join(HERE, name + ".py")
        new, out_new = measure([args.clpp, clp], args.runs)
        cpy, out_py = measure([sys.executable, py], args.runs)
        old = None
        if args.baseline:
            old, _ = measure([args.baseline, clp], args.runs)
        rows.append((name, old, new, cpy, out_new, out_py))
    if args.markdown:
        header = "| Benchmark | " + ("CL++ 0.9 antes (s) | " if args.baseline else "") + "CL++ agora (s) | CPython (s) | Saída |"
        print(header)
        print("|" + " --- |" * (5 if args.baseline else 4))
        for name, old, new, cpy, out_new, _ in rows:
            before = f"{old:.3f} | " if old is not None else ""
            print(f"| {name} | {before}{new:.3f} | {cpy:.3f} | `{out_new}` |")
    else:
        for name, old, new, cpy, out_new, out_py in rows:
            same = "ok" if out_new.replace(".0", "") == out_py.replace(".0", "") else "OUTPUT DIFFERS"
            print(f"{name:8} before={old if old is None else round(old, 3)} now={new:.3f}s cpython={cpy:.3f}s {same}")


if __name__ == "__main__":
    main()
