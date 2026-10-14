import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = os.path.abspath(sys.argv[1])
with tempfile.TemporaryDirectory() as folder:
    root = Path(folder)
    script = root / 'check.clp'
    script.write_text('link @clpp.fs as Fs; Fs.write("executed.txt", "unexpected");', encoding='utf-8')
    checked = subprocess.run([binary, '--check', str(script)], cwd=root, capture_output=True, text=True)
    assert checked.returncode == 0, checked.stderr
    assert not (root / 'executed.txt').exists(), 'check executed a native side effect'
    script.write_text('number Read(number value = 1) { return value; }', encoding='utf-8')
    rejected = subprocess.run([binary, '--check', str(script)], cwd=root, capture_output=True, text=True)
    assert rejected.returncode != 0 and 'default values' in rejected.stderr, rejected.stderr
    missing = subprocess.run([binary, '--check'], capture_output=True, text=True)
    assert missing.returncode != 0 and 'Usage:' in missing.stderr
