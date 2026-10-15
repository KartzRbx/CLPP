"""Format temporary copies over real LSP, then type-check every resulting source."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
from xml.etree import ElementTree as ET
from lsp_client import Client


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--junit', type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    repo = Path(__file__).resolve().parents[2]
    results = []
    with tempfile.TemporaryDirectory(prefix='clpp-format-') as directory:
        root = Path(directory)
        for name in ['examples', 'benchmarks']:
            shutil.copytree(repo / name, root / name)
        sources = sorted((root / 'examples').rglob('*.clp')) + sorted((root / 'benchmarks').glob('*.clp'))
        client = Client(str(compiler), root.as_posix())
        try:
            # VS Code passes indentation options and applies exactly this edit
            # range. Check both contracts, including a final UTF-16 surrogate.
            sample = 'if(true){post("ação 😀");}'
            for label, insert_spaces in [('four-spaces', True), ('tabs', False)]:
                started = time.monotonic()
                error = ''
                uri = client.open((root / f'{label}.clp').as_posix(), sample)
                try:
                    response = client.req('textDocument/formatting', {'textDocument': {'uri': uri},
                                          'options': {'tabSize': 4, 'insertSpaces': insert_spaces}}, timeout=20)
                    edits = response['result']
                    assert len(edits) == 1, edits
                    assert edits[0]['range']['end'] == {'line': 0, 'character': len(sample.encode('utf-16-le')) // 2}, edits
                    indent = '    ' if insert_spaces else '\t'
                    expected = f'if (true) {{\n{indent}post("ação 😀");\n}}\n'
                    assert edits[0]['newText'] == expected, repr(edits[0]['newText'])
                except Exception as failure:
                    error = str(failure) or type(failure).__name__
                finally:
                    client.notify('textDocument/didClose', {'textDocument': {'uri': uri}})
                results.append((f'contract/{label}', time.monotonic() - started, error))
                print('FAIL' if error else 'PASS', f'contract/{label}', error[:800], flush=True)
            generic = ('export def Predicate<T> = (T) -> bool;\n'
                       'export template<typename T> array<T> Filter(array<T> input, Predicate < T > predicate) {\n'
                       'array < T > output = array < T > ();\nreturn output;\n}\n')
            started = time.monotonic()
            error = ''
            uri = client.open((root / 'generic-highlight.clp').as_posix(), generic)
            try:
                names = client.caps['semanticTokensProvider']['legend']['tokenTypes']
                def roles(source):
                    result = client.req('textDocument/semanticTokens/full', {'textDocument': {'uri': uri}}, timeout=20)
                    values = result['result']['data']
                    line = column = 0
                    found = []
                    lines = source.splitlines()
                    for index in range(0, len(values), 5):
                        delta, char, length, kind, _ = values[index:index + 5]
                        line += delta
                        column = char if delta else column + char
                        word = lines[line][column:column + length]
                        if word in {'array', 'Predicate', 'T', 'Filter'}:
                            found.append((word, names[kind]))
                    return found
                before = roles(generic)
                expected = {'array': 'type', 'Predicate': 'type', 'T': 'typeParameter', 'Filter': 'function'}
                assert set(word for word, _ in before) == set(expected), before
                assert all(expected[word] == kind for word, kind in before), before
                formatted = client.req('textDocument/formatting', {'textDocument': {'uri': uri},
                                         'options': {'tabSize': 4, 'insertSpaces': True}}, timeout=20)['result'][0]['newText']
                assert 'array<T> output = array<T>();' in formatted, formatted
                client.change(uri, formatted, 2)
                assert roles(formatted) == before, 'formatting changed generic semantic roles'
            except Exception as failure:
                error = str(failure) or type(failure).__name__
            finally:
                client.notify('textDocument/didClose', {'textDocument': {'uri': uri}})
            results.append(('contract/generic-highlight', time.monotonic() - started, error))
            print('FAIL' if error else 'PASS', 'contract/generic-highlight', error[:800], flush=True)
            for source in sources:
                started = time.monotonic()
                error = ''
                uri = None
                try:
                    original = source.read_text(encoding='utf8')
                    uri = client.open(source.as_posix(), original)
                    response = client.req('textDocument/formatting', {'textDocument': {'uri': uri},
                                         'options': {'tabSize': 2, 'insertSpaces': True}}, timeout=20)
                    assert response and 'result' in response, response
                    edits = response['result']
                    assert len(edits) == 1, edits
                    formatted = edits[0]['newText']
                    source.write_text(formatted, encoding='utf8', newline='\n')
                    client.change(uri, formatted, 2)
                    again = client.req('textDocument/formatting', {'textDocument': {'uri': uri},
                                       'options': {'tabSize': 2, 'insertSpaces': True}}, timeout=20)
                    assert again and again['result'][0]['newText'] == formatted, 'formatter is not idempotent'
                    check = subprocess.run([str(compiler), '--check', str(source)], cwd=root,
                                           capture_output=True, text=True, encoding='utf8', timeout=30)
                    assert check.returncode == 0, check.stderr
                except Exception as failure:
                    error = str(failure) or type(failure).__name__
                finally:
                    if uri:
                        client.notify('textDocument/didClose', {'textDocument': {'uri': uri}})
                name = source.relative_to(root).as_posix()
                results.append((name, time.monotonic() - started, error))
                print('FAIL' if error else 'PASS', name, error[:800], flush=True)
        finally:
            client.close()
    suite = ET.Element('testsuite', name='formatter-real-lsp-corpus', tests=str(len(results)),
                       failures=str(sum(bool(error) for _, _, error in results)))
    for name, elapsed, error in results:
        item = ET.SubElement(suite, 'testcase', name=name, time=str(elapsed))
        if error:
            ET.SubElement(item, 'failure', message=error[:400]).text = error
    if args.junit:
        args.junit.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(suite).write(args.junit, encoding='utf8', xml_declaration=True)
    return int(any(error for _, _, error in results))


if __name__ == '__main__':
    raise SystemExit(main())
