"""Exercise the actual portable layout, version and script execution off-checkout."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    args = parser.parse_args()
    system = 'windows' if os.name == 'nt' else 'macos' if sys.platform == 'darwin' else 'linux'
    architecture = 'arm64' if platform.machine().lower() in ['arm64', 'aarch64'] else 'x64'
    with tempfile.TemporaryDirectory(prefix='clpp-release-archive-') as temporary:
        root = Path(temporary)
        subprocess.run([sys.executable, str(ROOT / 'tools/distribution/package_release.py'), str(args.compiler.resolve()),
                        '--platform', f'{system}-{architecture}', '--out', str(root)], check=True, timeout=60)
        archive_path, = root.glob('*.zip')
        installed = root / 'install ação'
        with zipfile.ZipFile(archive_path) as archive:
            assert archive.testzip() is None
            for entry in archive.infolist():
                assert (installed / entry.filename).resolve().is_relative_to(installed.resolve())
            archive.extractall(installed)
        metadata = json.loads((installed / 'release.json').read_text())
        assert metadata['product'] == 'CLPP-VM'
        binary = installed / metadata['executable']
        if os.name != 'nt':
            binary.chmod(0o755)
        version = subprocess.run([str(binary), '--version'], capture_output=True, text=True, check=True, timeout=15)
        assert metadata['version'] in version.stdout
        source = installed / 'check.clp'
        source.write_text('def Outcome = "ready" | "done"; post((Outcome "ready"));\n'
                          'template <typename T> T Identity(T value) { return value; }\npost(Identity<number>(42));', encoding='utf8')
        result = subprocess.run([str(binary), str(source)], cwd=installed, capture_output=True, text=True, encoding='utf8', timeout=20)
        assert result.returncode == 0 and result.stdout == 'ready\n42\n', (result.stdout, result.stderr)
        assert (installed / 'share/clpp/LICENSE').is_file()
        notices = (installed / 'share/clpp/THIRD_PARTY_NOTICES.txt').read_text(encoding='utf8')
        assert 'Niels Lohmann' in notices and 'LLVM' in notices and 'MinGW' in notices
        print('PASS portable runtime archive version, Unicode extraction, type ascription and generics')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
