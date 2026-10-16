"""Compile every example/module and reject deprecated spellings in executable code."""
from pathlib import Path
import re, subprocess, sys
root = Path(__file__).resolve().parents[2]
compiler = str(Path(sys.argv[1]).resolve())
sources = [*sorted((root / 'examples').rglob('*.clp')), *sorted((root / 'benchmarks').glob('*.clp'))]
legacy = re.compile(r'\b(?:struct|variant|import|from|using|namespace|new|let|mut|cout|endl|static)\b|\btype\s+[A-Za-z_]\w*\s*=|\benum\s+(?!class\b)|\bfunc\s+[A-Za-z_]\w*\s*\(|\b(?:task|signal)\s+[A-Za-z_]\w*\b|@(?!this\b|clpp\b)[A-Za-z_]\w*|@this\s*::|~>(?!\s*Connect\b)|\[\[')
skip = re.compile(r'<<\[.*?\]>>|<<[^\r\n]*|"(?:\\.|[^"\\])*"|`(?:\\.|[^`\\])*`', re.S)
failures = []
for source in sources:
    code = skip.sub(lambda m: '\n' * m.group().count('\n'), source.read_text(encoding='utf8'))
    deprecated = legacy.search(code)
    if deprecated:
        failures.append(f'{source.relative_to(root)}: legacy syntax {deprecated.group()!r}')
    result = subprocess.run([compiler, '--check', str(source)], cwd=root,
                            capture_output=True, text=True, encoding='utf8', timeout=30)
    if result.returncode:
        failures.append(f'{source.relative_to(root)}: {result.stderr}')
    else:
        print('PASS', source.relative_to(root).as_posix())
if failures:
    print('\n'.join(failures), file=sys.stderr)
    raise SystemExit(1)
print(f'{len(sources)} example and benchmark sources checked; no legacy spellings')
