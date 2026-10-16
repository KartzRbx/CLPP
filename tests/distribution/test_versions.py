"""CLI, LSP, VSIX and installer must describe the same release."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/lsp'))
from lsp_client import Client


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    args = parser.parse_args()
    version = re.search(r'project\(clpp VERSION ([\d.]+)', (ROOT / 'CMakeLists.txt').read_text()).group(1)
    reported = subprocess.run([str(args.compiler.resolve()), '--version'], capture_output=True, text=True, check=True, timeout=10)
    assert reported.stdout.strip().endswith(' ' + version), reported.stdout
    package = json.loads((ROOT / 'tools/vscode/package.json').read_text(encoding='utf8'))
    assert package['version'] == version
    assert f"$Version = '{version}'" in (ROOT / 'scripts/install-vm.ps1').read_text()
    with tempfile.TemporaryDirectory(prefix='clpp-version-') as temporary:
        client = Client(str(args.compiler.resolve()), Path(temporary).as_posix())
        try:
            assert client.server_info['version'] == version, client.server_info
            uri = client.open((Path(temporary) / 'test.clp').as_posix(), 'number value = "bad";')
            client.req('textDocument/documentSymbol', {'textDocument': {'uri': uri}})
            messages = [item['message'] for item in client.diags.get(uri, [])]
            assert 'type mismatch: expected number, received "bad"' in messages, messages
        finally:
            client.close()
    print('PASS CLI/LSP/editor/installer release identity and formatted LSP type diagnostic')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
