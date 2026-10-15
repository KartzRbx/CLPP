"""A deliberate assertion failure must produce complete XML without sanitizer errors."""
import subprocess
import sys
import tempfile
from pathlib import Path
from xml.etree import ElementTree as ET

with tempfile.TemporaryDirectory(prefix="clpp-reporter-") as directory:
    output = Path(directory) / "result.xml"
    result = subprocess.run([str(Path(sys.argv[1]).resolve()), "--reporter", "junit", "--out", str(output)],
                            capture_output=True, encoding="utf8", timeout=20)
    # Catch2 versions use either the generic failure status or testsFailed (42).
    assert result.returncode in (1, 42), (result.returncode, result.stderr)
    assert "AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr, result.stderr
    tree = ET.parse(output)
    assert len(tree.findall(".//failure")) == 1
    assert "Failure reports must remain valid" in tree.find(".//failure").text
    print("PASS deliberate assertion failure produces complete sanitizer-safe JUnit")
