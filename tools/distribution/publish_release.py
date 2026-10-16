"""Prepare checked CI artifacts; publication is an explicit final operation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('artifacts', type=Path)
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    version = re.search(r'project\(clpp VERSION ([\d.]+)', (ROOT / 'CMakeLists.txt').read_text()).group(1)
    stage = ROOT / 'build/release-publish' / version
    stage.mkdir(parents=True, exist_ok=True)
    files = {}
    platforms = set()
    for archive_path in sorted(args.artifacts.rglob(f'clpp-vm-{version}-*.zip')):
        data = archive_path.read_bytes()
        checksum = archive_path.with_suffix('.zip.sha256')
        assert checksum.is_file() and hashlib.sha256(data).hexdigest() == checksum.read_text().split()[0], archive_path
        with zipfile.ZipFile(archive_path) as archive:
            assert archive.testzip() is None
            metadata = json.loads(archive.read('release.json'))
            assert metadata['product'] == 'CLPP-VM' and metadata['version'] == version
            assert archive.read('share/clpp/THIRD_PARTY_NOTICES.txt') == (ROOT / 'THIRD_PARTY_NOTICES.txt').read_bytes()
            assert archive_path.name == f"clpp-vm-{version}-{metadata['platform']}.zip"
            platforms.add(metadata['platform'])
            if metadata['platform'] == 'windows-x64':
                files['clpp-vm.exe'] = archive.read('bin/clpp-vm.exe')
        assert archive_path.name not in files, 'duplicate runtime artifact'
        files[archive_path.name] = data
    assert {'windows-x64', 'linux-x64'}.issubset(platforms), platforms
    assert any(platform.startswith('macos-') for platform in platforms), platforms
    extensions = list(args.artifacts.rglob(f'clpp-language-{version}.vsix'))
    assert len(extensions) == 1, extensions
    with zipfile.ZipFile(extensions[0]) as archive:
        assert archive.testzip() is None
        package = json.loads(archive.read('extension/package.json'))
        assert package['version'] == version and package['publisher'] == 'kartzrbx'
        assert archive.read('extension/THIRD_PARTY_NOTICES.txt') == (ROOT / 'THIRD_PARTY_NOTICES.txt').read_bytes()
        assert archive.read('extension/bin/win32-x64/clpp-vm.exe') == files['clpp-vm.exe'], 'VSIX and portable runtime differ'
    files[extensions[0].name] = extensions[0].read_bytes()
    files['LICENSE'] = (ROOT / 'LICENSE').read_bytes()
    files['THIRD_PARTY_NOTICES.txt'] = (ROOT / 'THIRD_PARTY_NOTICES.txt').read_bytes()
    installer = (ROOT / 'scripts/install-vm.ps1').read_bytes()
    assert f"$Version = '{version}'".encode() in installer
    files['install-vm.ps1'] = installer
    for name, data in files.items():
        (stage / name).write_bytes(data)
    (stage / 'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(data).hexdigest()}  {name}\n' for name, data in sorted(files.items())), encoding='ascii')
    print(json.dumps(dict(version=version, platforms=sorted(platforms), files=sorted(files), stage=str(stage)), indent=2))
    if args.publish:
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        subprocess.run(['gh', 'release', 'create', 'v' + version, '--repo', 'KartzRbx/CLPP', '--target', revision,
                        '--title', 'CL++ VM ' + version, '--latest', '--notes-file', str(ROOT / f'.github/releases/v{version}.md'),
                        *[str(stage / name) for name in sorted(files)], str(stage / 'SHA256SUMS')], check=True)


if __name__ == '__main__':
    main()
