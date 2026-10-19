"""Validate a VSIX archive and execute its bundled compiler without SDK DLLs."""
import argparse
import hashlib
import json
import re
import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('vsix', type=Path)
    parser.add_argument('--compiler', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='clpp-package-check-') as directory:
        destination = Path(directory).resolve()
        with zipfile.ZipFile(args.vsix) as archive:
            assert archive.testzip() is None, 'corrupt archive member'
            for member in archive.infolist():
                target = (destination / member.filename).resolve()
                assert target.is_relative_to(destination), 'archive path escapes extraction root'
                archive.extract(member, destination)
        package = destination / 'extension'
        metadata = json.loads((package / 'package.json').read_text(encoding='utf8'))
        assert (package / metadata['main']).is_file(), 'missing extension entry point'
        assert (package / 'LICENSE.txt').is_file(), 'project license missing'
        assert 'Niels Lohmann' in (package / 'THIRD_PARTY_NOTICES.txt').read_text(encoding='utf8'), 'dependency notices missing'
        grammar = package / 'syntaxes/clpp.tmLanguage.json'
        rules = json.loads(grammar.read_text(encoding='utf8'))['repository']
        assert re.fullmatch(rules['type-name']['match'], '_Wallet'), 'underscore-prefixed type fallback missing'
        assert rules['comment-line']['begin'] == r'<<(?!\[)'
        assert rules['comment-block']['begin'] == r'<<\['
        assert rules['comment-block']['end'] == r'\]>>'
        ET.parse(destination / 'extension.vsixmanifest')
        compiler = package / 'bin/win32-x64/clpp-vm.exe'
        assert compiler.is_file(), 'missing bundled Windows compiler'
        if args.compiler:
            assert hashlib.sha256(compiler.read_bytes()).digest() == hashlib.sha256(args.compiler.read_bytes()).digest(), 'compiler changed while packaging'
        print('PASS archive, manifest, entry point, comments and bundled compiler')
        if os.name != 'nt':
            print('Windows execution requires a Windows runner; archive checks only')
            return
        environment = dict(os.environ)
        environment['PATH'] = str(Path(environment.get('SystemRoot', 'C:/Windows')) / 'System32')
        version = subprocess.run([str(compiler), '--version'], cwd=destination, env=environment,
                                 capture_output=True, text=True, timeout=10)
        assert version.returncode == 0 and 'clpp' in version.stdout.lower(), (version.returncode, version.stdout, version.stderr)
        source = destination / 'package-check.clp'
        source.write_text('''<< this heading is ignored: def return {} 0xFFFFFF
<<[ multiline comment: ] > } ]>>
def Choice<T> = T|string;
template <typename T> T Keep(Choice<T> value, T fallback) { return fallback; }
post(Keep("ignored", 42));
def Either<T,U> = T|U;
template <typename T, typename U> T Pick(Either<T,U> value, T fallback, U witness) { return fallback; }
post(Pick(true, 7, false));
''', encoding='utf8')
        result = subprocess.run([str(compiler), str(source)], cwd=destination, env=environment,
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0 and result.stdout == '42\n7\n', (result.returncode, result.stdout, result.stderr)
        source.write_text('post(' + '(' * 128 + '1' + ')' * 128 + ');', encoding='utf8')
        rejected = subprocess.run([str(compiler), '--check', str(source)], cwd=destination, env=environment,
                                  capture_output=True, text=True, timeout=10)
        assert rejected.returncode == 1 and 'syntax nesting limit exceeded' in rejected.stderr, (rejected.returncode, rejected.stderr)
        print('PASS extracted compiler executes current syntax with only system DLLs on PATH')


if __name__ == '__main__':
    main()
