"""Accept module completions exactly as an editor does, then compile the edits."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET
from lsp_client import Client


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--junit', type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    results = []
    with tempfile.TemporaryDirectory(prefix='clpp-auto-import-') as temporary:
        root = Path(temporary) / 'project ação'
        root.mkdir()
        for name in ['lib/NotificationModule.clp', 'one/Shared.clp', 'two/Shared.clp',
                     'lib/notice-tools.clp', 'build/HiddenGenerated.clp']:
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('export string Ping() { return "ok"; }', encoding='utf8')
        document = root / 'nested/Main.clp'
        document.parent.mkdir()
        client = Client(str(compiler), root.as_posix())
        version = 0

        def complete(marked):
            nonlocal version
            before, after = marked.split('|', 1)
            text = before + after
            version += 1
            client.notify('textDocument/didOpen', {'textDocument': {'uri': document.as_uri(), 'languageId': 'clpp', 'version': version, 'text': text}})
            response = client.req('textDocument/completion', {'textDocument': {'uri': document.as_uri()},
                                  'position': {'line': before.count('\n'), 'character': len(before.split('\n')[-1].encode('utf-16-le')) // 2}}, timeout=20)
            assert response and 'result' in response, response
            return text, response['result']['items']

        def offset(text, point):
            lines = text.splitlines(keepends=True)
            if point['line'] == len(lines):
                return len(text)
            prefix = lines[point['line']].encode('utf-16-le')[:point['character'] * 2].decode('utf-16-le')
            return sum(len(line) for line in lines[:point['line']]) + len(prefix)

        def accept(text, item):
            edits = [item['textEdit'], *item.get('additionalTextEdits', [])]
            edits = sorted([(offset(text, edit['range']['start']), offset(text, edit['range']['end']), edit['newText']) for edit in edits], reverse=True)
            for index, (start, end, new) in enumerate(edits):
                if index:
                    assert end <= edits[index - 1][0], 'overlapping completion edits'
                text = text[:start] + new + text[end:]
            document.write_text(text, encoding='utf8')
            check = subprocess.run([str(compiler), '--check', str(document)], capture_output=True, text=True, encoding='utf8', timeout=20)
            assert check.returncode == 0, check.stderr
            return text

        def auto(items, name):
            return [item for item in items if item['label'] == name and 'Auto-import:' in item.get('detail', '')]

        def run(name, callback):
            started = time.monotonic()
            error = ''
            try:
                callback()
            except Exception as failure:
                error = str(failure) or type(failure).__name__
            results.append((name, time.monotonic() - started, error))
            print('FAIL' if error else 'PASS', name, error[:600], flush=True)

        def project_module():
            text, items = complete('<< license\npost(Noti|.Ping());\n')
            selected = auto(items, 'NotificationModule')
            assert len(selected) == 1, selected
            formatted = accept(text, selected[0])
            assert formatted.startswith('<< license\nlink "../lib/NotificationModule.clp" as NotificationModule;\n'), formatted
            assert 'post(NotificationModule.Ping())' in formatted

        def first_word():
            text, items = complete('Noti|.Ping();')
            selected = auto(items, 'NotificationModule')[0]
            assert not selected.get('additionalTextEdits'), selected
            assert accept(text, selected).startswith('link "../lib/NotificationModule.clp" as NotificationModule;\n\nNotificationModule.Ping();')

        def native_module():
            text, items = complete('post(Jso|.Stringify(1));')
            selected = auto(items, 'Json')[0]
            assert 'link @clpp.json as Json;' in selected['detail']
            accept(text, selected)

        def suppression():
            for marked in ['post("Noti|");', '<< Noti|\n', 'auto value = owner.Noti|;',
                           'link "../lib/Noti|ficationModule.clp" as Existing;']:
                _, items = complete(marked)
                assert not auto(items, 'NotificationModule'), marked
            for marked in ['link "../lib/NotificationModule.clp" as NotificationModule;\npost(Noti|.Ping());',
                           'link "../lib/NotificationModule.clp" as Existing;\npost(Noti|.Ping());',
                           'string NotificationModule = "local";\npost(Noti|);']:
                _, items = complete(marked)
                assert not auto(items, 'NotificationModule'), marked

        def ambiguous():
            text, items = complete('post(Sha|.Ping());')
            selected = auto(items, 'Shared')
            assert len(selected) == 2, selected
            assert selected[0]['detail'] != selected[1]['detail']
            for item in selected:
                accept(text, item)

        def arbitrary_names():
            text, items = complete('post(notice_|.Ping());')
            assert 'notice-tools.clp' in auto(items, 'notice_tools')[0]['detail']
            accept(text, auto(items, 'notice_tools')[0])
            _, items = complete('HiddenGen|')
            assert not auto(items, 'HiddenGenerated')
            _, items = complete('Mai|')
            assert not auto(items, 'Main'), 'self import suggested'

        def watcher():
            _, items = complete('Lat|')
            assert not auto(items, 'LaterModule')
            file = root / 'LaterModule.clp'
            file.write_text('export number Value() { return 1; }', encoding='utf8')
            client.notify('workspace/didChangeWatchedFiles', {'changes': [{'uri': file.as_uri(), 'type': 1}]})
            _, items = complete('Lat|')
            assert auto(items, 'LaterModule')
            file.unlink()
            client.notify('workspace/didChangeWatchedFiles', {'changes': [{'uri': file.as_uri(), 'type': 3}]})
            _, items = complete('Lat|')
            assert not auto(items, 'LaterModule')

        def crlf_and_existing_links():
            text, items = complete('<< license\r\nlink @clpp.io as Io; << existing\r\n\r\npost(Noti|.Ping());\r\n')
            result = accept(text, auto(items, 'NotificationModule')[0])
            assert 'Io; << existing\r\nlink "../lib/NotificationModule.clp" as NotificationModule;\r\n' in result, result

        def analysis_invalidation():
            file = root / 'lib/Dynamic.clp'
            file.write_text('export number First() { return 1; }', encoding='utf8')
            complete('link "../lib/Dynamic.clp" as Dynamic;\npost(Dynamic.|);')
            def names():
                response = client.req('textDocument/completion', {'textDocument': {'uri': document.as_uri()},
                                     'position': {'line': 1, 'character': 13}}, timeout=20)
                return {item['label'] for item in response['result']['items']}
            assert 'First' in names()
            for _ in range(3):
                assert 'First' in names(), 'unchanged cached graph changed results'
            client.notify('textDocument/didOpen', {'textDocument': {'uri': file.as_uri(), 'languageId': 'clpp', 'version': 1,
                          'text': 'export number Second() { return 2; }'}})
            assert 'Second' in names() and 'First' not in names(), 'unsaved dependency was not invalidated'
            client.notify('textDocument/didClose', {'textDocument': {'uri': file.as_uri()}})
            assert 'First' in names() and 'Second' not in names(), 'closing dependency did not restore disk content'
            file.write_text('export number Third() { return 3; }', encoding='utf8')
            client.notify('workspace/didChangeWatchedFiles', {'changes': [{'uri': file.as_uri(), 'type': 2}]})
            assert 'Third' in names() and 'First' not in names(), 'disk dependency change retained stale analysis'

        try:
            for name, callback in [('workspace module and Unicode path', project_module), ('first word nonoverlapping edits', first_word),
                                   ('native module', native_module), ('comments strings members imports and collisions', suppression),
                                   ('same name in different directories', ambiguous), ('arbitrary script names and ignored directories', arbitrary_names),
                                   ('create and delete cache invalidation', watcher), ('CRLF and existing import comments', crlf_and_existing_links),
                                   ('analysis cache tracks unsaved closed and changed dependencies', analysis_invalidation)]:
                run(name, callback)
        finally:
            client.close()
    suite = ET.Element('testsuite', name='auto-import-real-lsp', tests=str(len(results)), failures=str(sum(bool(error) for _, _, error in results)))
    for name, elapsed, error in results:
        case = ET.SubElement(suite, 'testcase', name=name, time=str(elapsed))
        if error:
            ET.SubElement(case, 'failure', message=error[:400]).text = error
    if args.junit:
        ET.ElementTree(suite).write(args.junit, encoding='utf8', xml_declaration=True)
    return int(any(error for _, _, error in results))


if __name__ == '__main__':
    raise SystemExit(main())
