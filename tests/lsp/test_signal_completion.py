"""Regression: signal scope completion must offer real, typed event operations."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from lsp_client import Client


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--junit', type=Path)
    args = parser.parse_args()
    results = []
    with tempfile.TemporaryDirectory(prefix='clpp-signal-completion-') as folder:
        root = Path(folder)
        (root / 'Human.clp').write_text('export def Human { string name; }\n', encoding='utf8')
        client = Client(str(args.compiler.resolve()), root.as_posix())
        uri = client.open((root / 'main.clp').as_posix(), '')
        version = 1

        def completion(source):
            nonlocal version
            version += 1
            client.change(uri, source, version)
            lines = source.split('\n')
            response = client.pos(uri, 'textDocument/completion', len(lines)-1, len(lines[-1]))
            assert response and 'result' in response, response
            value = response['result']
            return value['items'] if isinstance(value, dict) else value

        def check(name, source, expected, excluded=(), typed=False, colon_insertion=False):
            try:
                items = completion(source)
                by_name = {item['label']: item for item in items}
                assert set(expected) <= by_name.keys(), list(by_name)
                assert not set(excluded) & by_name.keys(), list(by_name)
                for label in expected:
                    if label != 'Connected': assert by_name[label]['kind'] == 2, by_name[label]
                if typed:
                    item = by_name['Connect']
                    assert all(word in item['detail'] for word in ['number', 'string', 'connection']), item
                    assert 'func(number ' in item['insertText'] and 'string ' in item['insertText'], item
                    assert item['insertTextFormat'] == 2, item
                if colon_insertion:
                    assert by_name['Connect']['insertText'].startswith(':Connect('), by_name['Connect']
                if typed or colon_insertion:
                    insertion = re.sub(r'\$\{\d+:([^}]*)\}', r'\1', by_name['Connect']['insertText'])
                    insertion = re.sub(r'\$\d+', '', insertion)
                    start = len(source)
                    while start and (source[start-1].isalnum() or source[start-1] == '_'):
                        start -= 1
                    accepted = source[:start] + insertion + ';\n'
                    path = root / 'accepted.clp'
                    path.write_text(accepted, encoding='utf8')
                    result = subprocess.run([str(args.compiler.resolve()), '--check', str(path)],
                                            capture_output=True, text=True, timeout=15)
                    assert result.returncode == 0, (accepted, result.stdout, result.stderr)
                results.append((name, ''))
                print('PASS', name)
            except Exception as error:
                results.append((name, str(error)))
                print('FAIL', name, str(error))

        try:
            methods = ['Connect', 'Once', 'Fire', 'Clear', 'Wait']
            check('signal scope offers its operations', 'signal<number, string> changed;\nchanged::', methods, ['is'], True)
            check('signal prefix filters operations', 'signal<number> changed;\nchanged::Con', ['Connect'], ['is', 'Fire'])
            check('signal single colon offers valid scope operations and type test',
                  'signal<number> changed;\nchanged:', ['is'] + methods, colon_insertion=True)
            check('signal first-colon prefix inserts missing colon',
                  'signal<number> changed;\nchanged:Con', ['Connect'], ['is', 'Fire'], colon_insertion=True)
            check('function signal parameter', 'void Subscribe(signal<number> changed) {\nchanged::', methods)
            check('local signal declaration', 'void Subscribe() {\nsignal<number> changed;\nchanged::', methods)
            check('empty payload signal', 'signal<> changed;\nchanged::', methods)
            check('constructed signal infers payload', 'auto changed = signal<number, string>();\nchanged::', methods, typed=True)
            check('nested generic payload', 'signal<tuple<number, string>> changed;\nchanged::', methods)
            check('signal type alias', 'def Changed = signal<number, string>;\nChanged changed = signal<number, string>();\nchanged::', methods)
            check('imported signal payload', 'link "./Human.clp" as H;\nsignal<H.Human, string> joined;\njoined::', methods)
            check('explicit connection scope', 'signal<number> changed;\nconnection subscription = changed::Connect(func(number value) -> void {});\nsubscription::', ['Disconnect', 'Connected'], ['Connect'])
            check('inferred connection scope', 'signal<number> changed;\nauto subscription = changed::Connect(func(number value) -> void {});\nsubscription::', ['Disconnect', 'Connected'], ['Connect'])
            check('connection dot exposes status', 'signal<number> changed;\nconnection subscription = changed::Connect(func(number value) -> void {});\nsubscription.', ['Connected'])
            check('unrelated values have no event methods', 'number value = 1;\nvalue::', [], methods)
            check('shadowed signal does not leak methods',
                  'signal<number> changed;\nvoid Subscribe() {\nnumber changed = 1;\nchanged::', [], methods)
        finally:
            client.close()
    if args.junit:
        suite = ET.Element('testsuite', name='signal-completion', tests=str(len(results)), failures=str(sum(bool(e) for _, e in results)))
        for name, error in results:
            case = ET.SubElement(suite, 'testcase', name=name)
            if error: ET.SubElement(case, 'failure', message=error).text = error
        args.junit.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(suite).write(args.junit, encoding='utf8', xml_declaration=True)
    return 1 if any(error for _, error in results) else 0


if __name__ == '__main__':
    raise SystemExit(main())
