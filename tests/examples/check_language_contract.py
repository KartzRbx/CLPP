"""Check runnable language examples, rejection contracts and keyword documentation."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from xml.etree import ElementTree as ET


def main():
    sys.stdout.reconfigure(encoding="utf8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("--junit", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    compiler = args.compiler.resolve()
    folder = root / "examples/language"
    cases = json.loads((folder / "cases.json").read_text(encoding="utf8"))
    results = []

    def record(name, callback):
        try:
            callback()
        except Exception as error:
            results.append((name, str(error)))
            print("FAIL", name, str(error))
        else:
            results.append((name, None))
            print("PASS", name)

    def inventory():
        lexer = (root / "src/core/lexer/lexer.cpp").read_text(encoding="utf8")
        words = re.findall(r'KeywordEntry\{"([^"]+)"', lexer)
        text = (root / "docs/reference/keyword-inventory.md").read_text(encoding="utf8")
        documented = re.findall(r"^\| `([^`]+)` \| (?:current|removed) \|", text, re.M)
        assert len(documented) == len(set(documented)), "Duplicate keyword entries"
        assert set(words) == set(documented), (
            f"Missing: {set(words) - set(documented)}; stale: {set(documented) - set(words)}"
        )
        sources = {p.name for p in folder.glob("*.clp")}
        listed = [case["file"] for case in cases["positive"]]
        assert len(listed) == len(set(listed)), "Duplicate example entries"
        assert sources == set(listed), "Every entry program needs an expected output"

    record("keyword and example inventory", inventory)

    def run(source, *arguments):
        return subprocess.run([str(compiler), *arguments, str(source)], cwd=root,
                              capture_output=True, encoding="utf8", timeout=30)

    for case in cases["positive"]:
        def positive(case=case):
            source = folder / case["file"]
            checked = run(source, "--check")
            assert checked.returncode == 0, checked.stderr
            result = run(source)
            assert result.returncode == 0, result.stderr
            assert not result.stderr, f"Unexpected stderr: {result.stderr}"
            assert result.stdout == case["stdout"], (
                f"Expected {case['stdout']!r}; received {result.stdout!r}"
            )
        record(case["file"], positive)

    with tempfile.TemporaryDirectory(prefix="clpp-contract-") as temporary:
        source = Path(temporary) / "invalid.clp"
        for case in cases["negative"]:
            def negative(case=case):
                source.write_text(case["source"], encoding="utf8")
                result = run(source, "--check")
                assert result.returncode == 1, (
                    f"Expected compiler rejection, received {result.returncode}: {result.stderr}"
                )
                assert result.stderr.strip(), "Rejection must explain the failure"
                assert "AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr, result.stderr
            record("reject " + case["name"], negative)

    failures = sum(error is not None for _, error in results)
    if args.junit:
        suite = ET.Element("testsuite", name="language-contract", tests=str(len(results)),
                           failures=str(failures), errors="0")
        for name, error in results:
            item = ET.SubElement(suite, "testcase", classname="language-contract", name=name)
            if error:
                ET.SubElement(item, "failure", message=error[:300]).text = error
        args.junit.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(suite).write(args.junit, encoding="utf8", xml_declaration=True)
    print(f"{len(results) - failures}/{len(results)} language contract checks passed")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
