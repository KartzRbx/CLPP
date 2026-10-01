"""Every code example in the documentation is a test.

Rules for code fences in docs/**/*.md:

  ```clp                 -> must compile and run without errors
  ```clp                 followed right away by an ```output fence -> stdout must match it exactly
  ```clp error           followed by ```output -> must fail; the first error message must contain that text
  ```clp file=name.clp   -> written to the example folder (for `link "./name.clp"`), not run
  ```clp fragment        -> shown for reading only (not run)

The Portuguese markers of the first version of the guide (saida, erro, arquivo=, trecho) are
still accepted.

Programs run with CLPP_HEADLESS=3, so examples that open a window (Window.Open) run three frames
off screen and exit instead of waiting for someone to close the window.

Usage: python tests/docs/check_docs.py <clpp> [docs folder]
"""
import os
import re
import subprocess
import sys
import tempfile

FENCE = re.compile(r"```([^\n`]*)\n(.*?)```", re.S)
OUTPUT = ("output", "saida")


def blocks(text):
    found = [(m.group(1).strip(), m.group(2), m.start(), m.end()) for m in FENCE.finditer(text)]
    for index, (info, body, start, end) in enumerate(found):
        following = found[index + 1] if index + 1 < len(found) else None
        expected = None
        if following and following[0] in OUTPUT and text[end:following[2]].strip() == "":
            expected = following[1]
        yield info, body, expected, text.count("\n", 0, start) + 1


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    clpp = os.path.abspath(sys.argv[1])
    root = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "..", "docs")
    # Enough frames that a simulated press-and-release (down, render, up, render) completes in a
    # windowed example; examples that just draw a frame and exit are unaffected.
    environment = dict(os.environ, CLPP_HEADLESS="8")
    failures = checked = 0
    for folder, _, files in os.walk(root):
        for name in sorted(files):
            if not name.endswith(".md"):
                continue
            path = os.path.join(folder, name)
            with open(path, encoding="utf-8") as f:
                text = f.read()
            with tempfile.TemporaryDirectory() as work:
                pending = []
                for info, body, expected, line in blocks(text):
                    words = info.split()
                    if not words or words[0] != "clp":
                        continue
                    options = words[1:]
                    target = next((o.split("=", 1)[1] for o in options if o.startswith(("file=", "arquivo="))), None)
                    if target:
                        with open(os.path.join(work, target), "w", encoding="utf-8", newline="\n") as f:
                            f.write(body)
                        continue
                    if "fragment" in options or "trecho" in options:
                        continue
                    pending.append((options, body, expected, line))
                for index, (options, body, expected, line) in enumerate(pending):
                    source = os.path.join(work, f"example_{index}.clp")
                    with open(source, "w", encoding="utf-8", newline="\n") as f:
                        f.write(body)
                    result = subprocess.run([clpp, source], capture_output=True, encoding="utf-8", errors="replace",
                                            cwd=work, timeout=60, env=environment)
                    checked += 1
                    where = f"{os.path.relpath(path, root)}:{line}"
                    if "error" in options or "erro" in options:
                        ok = result.returncode != 0 and (expected is None or expected.strip() in result.stderr)
                        detail = result.stderr.strip()
                    else:
                        stdout = result.stdout.replace("\r\n", "\n")
                        ok = result.returncode == 0 and (expected is None or stdout == expected)
                        detail = (result.stderr.strip() + "\n" + stdout).strip()
                    if not ok:
                        failures += 1
                        print(f"FAIL {where}\n  expected:\n{expected}\n  got:\n{detail}\n")
    print(f"{checked} examples checked, {failures} failure(s)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
