"""CL++ benchmarks: median wall time of N runs per program, next to CPython.

Usage: python benchmarks/run.py <clpp> [--baseline <old clpp>] [--runs 5] [--markdown]
Each benchmark exists as <name>.clp and <name>.py with the same work and the same output.
"""
import argparse
import math
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
        if result.returncode != 0:
            raise RuntimeError(f"benchmark failed ({result.returncode}): {command}\n{result.stderr}")
        times.append(time.perf_counter() - start)
        current = result.stdout.strip()
        if output is not None and current != output:
            raise RuntimeError(f"unstable output: {command}\n{output!r} != {current!r}")
        output = current
    return statistics.median(times), output


def outputs_match(actual, expected):
    if actual == expected:
        return True
    try:
        # Only a single finite numeric result can differ in float formatting.
        left, right = float(actual), float(expected)
        return math.isfinite(left) and math.isfinite(right) and math.isclose(left, right, rel_tol=1e-9, abs_tol=1e-9)
    except ValueError:
        return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("clpp")
    parser.add_argument("--baseline")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--markdown", action="store_true")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    rows = []
    for name in NAMES:
        clp = os.path.join(HERE, name + ".clp")
        py = os.path.join(HERE, name + ".py")
        new, out_new = measure([args.clpp, clp], args.runs)
        checked, _ = measure([args.clpp, '--check', clp], args.runs)
        cpy, out_py = measure([sys.executable, py], args.runs)
        old = None
        if args.baseline:
            old, out_old = measure([args.baseline, clp], args.runs)
            if not outputs_match(out_old, out_py):
                raise RuntimeError(f"{name}: baseline output differs: {out_old!r} != {out_py!r}")
        if not outputs_match(out_new, out_py):
            raise RuntimeError(f"{name}: output differs: {out_new!r} != {out_py!r}")
        rows.append((name, old, new, cpy, checked, out_new, out_py))
    print("End-to-end process wall time (includes startup and CL++ compilation); all outputs verified.")
    if args.markdown:
        header = "| Benchmark | " + ("Baseline (s) | " if args.baseline else "") + "CL++ execution (s) | CPython (s) | CL++ --check (s) | Output |"
        print(header)
        print("|" + " --- |" * (6 if args.baseline else 5))
        for name, old, new, cpy, checked, out_new, _ in rows:
            before = f"{old:.3f} | " if old is not None else ""
            print(f"| {name} | {before}{new:.3f} | {cpy:.3f} | {checked:.3f} | `{out_new}` |")
    else:
        for name, old, new, cpy, checked, out_new, out_py in rows:
            same = "ok"
            print(f"{name:8} before={old if old is None else round(old, 3)} now={new:.3f}s cpython={cpy:.3f}s check={checked:.3f}s {same}")


if __name__ == "__main__":
    main()
