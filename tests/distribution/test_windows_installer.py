"""Install, upgrade, reject bad inputs, and uninstall without removing user files."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--previous-archive', type=Path)
    parser.add_argument('--previous-checksums', type=Path)
    args = parser.parse_args()
    if bool(args.previous_archive) != bool(args.previous_checksums):
        parser.error('previous archive and checksums must be provided together')
    shell = shutil.which('pwsh') or shutil.which('powershell')
    assert shell, 'PowerShell required on Windows'
    with tempfile.TemporaryDirectory(prefix='clpp-installer-contract-') as temporary:
        root = Path(temporary)
        subprocess.run([sys.executable, str(ROOT / 'tools/distribution/package_release.py'), str(args.compiler.resolve()),
                        '--platform', 'windows-x64', '--out', str(root)], check=True, timeout=60)
        archive, = root.glob('*.zip')
        digest = archive.with_suffix('.zip.sha256')
        version = archive.name.removeprefix('clpp-vm-').removesuffix('-windows-x64.zip')
        prefix = root / 'install ação'
        prefix.mkdir()
        user_file = prefix / 'my-project.clp'
        user_file.write_text('post("keep me");', encoding='utf8')
        command = [shell, '-NoLogo', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', str(ROOT / 'scripts/install-vm.ps1'), '-Prefix', str(prefix)]

        def run(*arguments, expected=0):
            result = subprocess.run([*command, *arguments], capture_output=True, text=True, encoding='utf8', errors='replace', timeout=90)
            assert (result.returncode == 0) == (expected == 0), (arguments, result.stdout, result.stderr)
            return result

        options = ['-Version', version, '-ArchivePath', str(archive), '-ChecksumsPath', str(digest)]
        outside = root / 'unrelated'
        outside.mkdir()
        sentinel = outside / 'clpp-vm.exe'
        sentinel.write_bytes(b'keep this unrelated executable')
        junction = prefix / 'bin'
        subprocess.run(['cmd', '/c', 'mklink', '/J', str(junction), str(outside)],
                       capture_output=True, check=True, timeout=15)
        try:
            result = run(*options, '-AdoptExisting', expected=1)
            assert 'junctions' in result.stderr
            assert sentinel.read_bytes() == b'keep this unrelated executable'
        finally:
            junction.rmdir()
        print('PASS directory junction rejected without changing unrelated files')
        foreign = prefix / 'INSTALL.md'
        foreign.write_text('user-owned installation notes', encoding='utf8')
        run(*options, expected=1)
        assert foreign.read_text(encoding='utf8') == 'user-owned installation notes'
        foreign.unlink()
        print('PASS unowned filename collision rejected without overwriting data')
        if args.previous_archive:
            with zipfile.ZipFile(args.previous_archive) as previous:
                metadata = json.loads(previous.read('release.json'))
                assert metadata['product'] == 'CLPP-VM' and metadata['platform'] == 'windows-x64'
                previous_hash = hashlib.sha256(previous.read('bin/clpp-vm.exe')).hexdigest()
            run('-Version', metadata['version'], '-ArchivePath', str(args.previous_archive.resolve()),
                '-ChecksumsPath', str(args.previous_checksums.resolve()))
            assert hashlib.sha256((prefix / 'bin/clpp-vm.exe').read_bytes()).hexdigest() == previous_hash
        run(*options)
        binary = prefix / 'bin/clpp-vm.exe'
        original = hashlib.sha256(binary.read_bytes()).hexdigest()
        assert original == hashlib.sha256(args.compiler.read_bytes()).hexdigest()
        if args.previous_archive:
            assert json.loads((prefix / '.clpp-vm-install.json').read_text(encoding='utf8'))['version'] == version
            assert user_file.read_text(encoding='utf8') == 'post("keep me");'
            print('PASS previous public release upgraded to candidate without losing user files')
        run(*options)
        assert hashlib.sha256(binary.read_bytes()).hexdigest() == original
        print('PASS install and same-version upgrade')
        bad = root / 'bad-checksums'
        bad.write_text('0' * 64 + '  ' + archive.name + '\n')
        result = run('-Version', version, '-ArchivePath', str(archive), '-ChecksumsPath', str(bad), expected=1)
        assert 'checksum mismatch' in result.stderr
        assert hashlib.sha256(binary.read_bytes()).hexdigest() == original
        print('PASS bad checksum rejected without altering installed compiler')
        manifest = prefix / '.clpp-vm-install.json'
        snapshot = manifest.read_bytes()
        manifest.write_text(json.dumps(dict(product='OtherProgram')), encoding='utf8')
        run(*options, expected=1)
        run('-Uninstall', expected=1)
        assert binary.is_file()
        manifest.write_bytes(snapshot)
        run('-Uninstall')
        assert not binary.exists() and not manifest.exists()
        assert user_file.read_text(encoding='utf8') == 'post("keep me");'
        print('PASS uninstall checks owner and preserves unrelated files')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
