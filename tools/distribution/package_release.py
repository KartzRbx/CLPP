"""Reproducible runtime ZIP layout, independent of compiler SDK installation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import platform
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--platform', default='auto', choices=['auto', 'windows-x64', 'linux-x64', 'macos-x64', 'macos-arm64'])
    parser.add_argument('--out', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    if args.platform == 'auto':
        system = 'windows' if sys.platform == 'win32' else 'macos' if sys.platform == 'darwin' else 'linux'
        arch = 'arm64' if platform.machine().lower() in ['arm64', 'aarch64'] else 'x64'
        args.platform = system + '-' + arch
    version = re.search(r'project\(clpp VERSION ([\d.]+)', (ROOT / 'CMakeLists.txt').read_text()).group(1)
    name = 'clpp-vm.exe' if args.platform.startswith('windows') else 'clpp-vm'
    if args.compiler.is_dir():
        candidates = [args.compiler / 'src' / candidate for candidate in ['clpp-vm.exe', 'clpp-vm', 'clpp.exe', 'clpp']]
        args.compiler = next((candidate for candidate in candidates if candidate.is_file()), args.compiler)
    assert args.compiler.is_file(), args.compiler
    args.out.mkdir(parents=True, exist_ok=True)
    target = args.out / f'clpp-vm-{version}-{args.platform}.zip'
    instructions = (f'CL++ VM {version} ({args.platform})\n\n'
                    f'Run bin/{name} --version, then bin/{name} script.clp.\n'
                    'Add the bin directory to PATH if desired. No administrator access is required.\n'
                    'Keep this VM separate from the independent clpp Luau compiler.\n'
                    'The VS Code extension is a separate VSIX asset.\n'
                    'To uninstall a portable extraction, remove only its own extracted directory.\n'
                    'Documentation: https://kartzrbx.github.io/CLPP/\n')
    files = {f'bin/{name}': args.compiler.read_bytes(), 'share/clpp/LICENSE': (ROOT / 'LICENSE').read_bytes(),
             'share/clpp/THIRD_PARTY_NOTICES.txt': (ROOT / 'THIRD_PARTY_NOTICES.txt').read_bytes(),
             'INSTALL.md': instructions.encode('utf8'),
             'release.json': (json.dumps(dict(product='CLPP-VM', version=version, platform=args.platform,
                                              executable=f'bin/{name}'), indent=2) + '\n').encode('utf8')}
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path, data in files.items():
            info = zipfile.ZipInfo(path, date_time=(2026, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (0o100755 if path.startswith('bin/') else 0o100644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_suffix('.zip.sha256').write_text(f'{digest}  {target.name}\n', encoding='ascii')
    print(target)
    print(digest)


if __name__ == '__main__':
    main()
