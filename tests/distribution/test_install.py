"""Install the CLI into an isolated prefix and execute it outside the checkout."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('build', type=Path)
    parser.add_argument('--archive', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='clpp-install-check-') as directory:
        prefix = Path(directory)
        if args.archive:
            with zipfile.ZipFile(args.archive) as archive:
                assert archive.testzip() is None, 'corrupt distribution archive'
                for member in archive.infolist():
                    assert (prefix / member.filename).resolve().is_relative_to(prefix), 'unsafe archive path'
                archive.extractall(prefix)
            binaries = list(prefix.rglob('clpp-vm.exe' if os.name == 'nt' else 'clpp-vm'))
            assert len(binaries) == 1, 'expected one installed compiler'
            prefix = binaries[0].parent.parent
        else:
            subprocess.run(['cmake', '--install', str(args.build.resolve()), '--prefix', str(prefix),
                            '--component', 'Runtime'], check=True, timeout=60)
        compiler = prefix / 'bin' / ('clpp-vm.exe' if os.name == 'nt' else 'clpp-vm')
        assert compiler.is_file(), 'installed CLI missing'
        assert (prefix / 'share/clpp/LICENSE').is_file(), 'license missing'
        environment = dict(os.environ)
        if os.name == 'nt':
            windows = next((value for key, value in environment.items()
                            if key.casefold() == 'systemroot'), 'C:/Windows')
            environment['PATH'] = str(Path(windows) / 'System32')
        result = subprocess.run([str(compiler), '--version'], cwd=prefix, env=environment,
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0 and 'clpp' in result.stdout.lower(), result
        source = prefix / 'installed.clp'
        source.write_text('<< installed CLI\ndef Box<T> { T Value; }\n'
                          'auto value = Box<number> { Value = 42 }; post(value.Value);\n', encoding='utf8')
        result = subprocess.run([str(compiler), str(source)], cwd=prefix, env=environment,
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0 and result.stdout == '42\n', (result.returncode, result.stderr)
        print('PASS isolated CLI installation, license and execution outside checkout')


if __name__ == '__main__':
    main()
