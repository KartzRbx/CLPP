"""Editor recovery, safe refactoring and compiler/editor parity over real LSP."""
import argparse
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from lsp_client import Client


def main():
    sys.stdout.reconfigure(encoding='utf8', errors='replace')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--junit', type=Path)
    parser.add_argument('--large', action='store_true', help='also measure 10,000 file bindings')
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    repo = Path(__file__).resolve().parents[2]
    results = []

    def run(name, callback):
        started = time.monotonic()
        try:
            callback()
            error = ''
        except Exception as failure:
            error = str(failure) or type(failure).__name__
        results.append((name, time.monotonic() - started, error))
        print('FAIL' if error else 'PASS', name, error[:1500], flush=True)

    with tempfile.TemporaryDirectory(prefix='clpp-editor-audit-') as directory:
        root = Path(directory)
        shutil.copytree(repo / 'examples/language', root / 'language')
        (root / 'Human.clp').write_text('export def Human { string name; optional<number> cpf; }', encoding='utf8')
        client = Client(str(compiler), root.as_posix())
        path = root / 'main.clp'
        uri = client.open(path.as_posix(), '')
        version = 1

        def edit(marked):
            nonlocal version
            offset = marked.find('@@')
            before = marked[:offset] if offset >= 0 else marked
            source = marked.replace('@@', '')
            version += 1
            client.change(uri, source, version)
            point = {'line': before.count('\n'), 'character': len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}
            return source, point

        def request(method, point=None, **kwargs):
            params = {'textDocument': {'uri': uri}, **kwargs}
            if point is not None:
                params['position'] = point
            response = client.req('textDocument/' + method, params, timeout=30)
            assert response is not None, 'request did not finish'
            return response

        def completion(marked):
            _, point = edit(marked)
            response = request('completion', point)
            assert 'result' in response, response
            result = response['result'] or {}
            return result.get('items', []) if isinstance(result, dict) else result

        def barrier():
            request('documentSymbol')
            return client.diags.get(uri, [])

        def compile_source(source, accepted):
            path.write_text(source, encoding='utf8')
            result = subprocess.run([str(compiler), '--check', str(path)], capture_output=True,
                                    encoding='utf8', timeout=30)
            assert result.returncode == (0 if accepted else 1), result.stderr

        try:
            for operator in ['::Connect', '~>Connect', '::Once']:
                def callback(operator=operator):
                    items = completion('link "./Human.clp" as H;\nsignal<H.Human, string> joined;\n'
                                       f'joined{operator}(func(H.Human person, string reason) -> void {{ post(person.@@); }});')
                    assert {'name', 'cpf'} <= {item['label'] for item in items}, items
                run('imported explicit signal callback ' + operator, callback)

            for tail in ['post(person.@@)', 'post(person.na@@)', 'if (person.@@) {',
                         'post(person.@@);\nnumber Later() { return 2; }']:
                def incomplete(tail=tail):
                    items = completion('link "./Human.clp" as H;\nH.Human person = H.Human { name = "Ada" };\n' + tail)
                    assert 'name' in {item['label'] for item in items}, items
                    assert not any(item['label'] == 'Later' for item in items), items
                run('incomplete member ' + tail, incomplete)

            def signature():
                _, point = edit('number Add(number first, string second) { return first; }\npost(Add(1, @@');
                result = request('signatureHelp', point)['result']
                assert result and result['activeParameter'] == 1 and 'second' in result['signatures'][0]['label'], result
            run('unfinished call retains signature and active parameter', signature)

            references = ('number score = 1;\nvoid Update() { number score = 2; post(score); }\n'
                          'score = score + 1;\npost(sc@@ore);\n<< score\npost("score");\n')
            def reference_check(include):
                _, point = edit(references)
                result = request('references', point, context={'includeDeclaration': include})['result']
                locations = [entry['range']['start']['line'] for entry in result]
                assert sorted(locations) == ([0, 2, 2, 3] if include else [2, 2, 3]), result
            run('references preserve shadowing and include the declaration on request', lambda: reference_check(True))
            run('references exclude the declaration on request', lambda: reference_check(False))

            def unresolved_reference():
                _, point = edit('void Work(number target) { post(tar@@get); post(missing); }')
                response = request('references', point, context={'includeDeclaration': True})['result']
                assert len(response) == 2, response
                renamed = request('rename', point, newName='value')['result']
                assert len(renamed['changes'][uri]) == 2, renamed
            run('unresolved names cannot alias the first parameter during refactoring', unresolved_reference)
            def out_of_scope_reference():
                _, point = edit('void Work() { if (true) { number target = 1; post(target); } post(tar@@get); }')
                response = request('references', point, context={'includeDeclaration': True})['result']
                assert not response, response
                assert request('prepareRename', point)['result'] is None
            run('out-of-scope names do not acquire references from an expired binding', out_of_scope_reference)

            def rename():
                source, point = edit(references)
                result = request('rename', point, newName='pontos')['result']
                changes = result['changes'][uri]
                assert len(changes) == 4 and all(edit['newText'] == 'pontos' for edit in changes), changes
                lines = source.splitlines(keepends=True)
                for change in sorted(changes, key=lambda item: (item['range']['start']['line'], item['range']['start']['character']), reverse=True):
                    start, end = change['range']['start'], change['range']['end']
                    line = lines[start['line']]
                    lines[start['line']] = line[:start['character']] + change['newText'] + line[end['character']:]
                renamed = ''.join(lines)
                assert 'number score = 2; post(score)' in renamed and '<< score' in renamed and '"score"' in renamed
                compile_source(renamed, True)
            run('rename edits only the selected binding and produces compilable code', rename)
            for source in [
                'number target = 1; number taken = 2; post(tar@@get);',
                'number target = 1; void Work() { number taken = 2; post(tar@@get); }',
                'number taken = 1; void Work() { number target = 2; post(tar@@get); post(taken); }',
            ]:
                def conflict(source=source):
                    _, point = edit(source)
                    response = request('rename', point, newName='taken')
                    assert response.get('error', {}).get('code') == -32602, response
                run('rename rejects duplicate or captured bindings ' + source, conflict)
            def unicode_rename():
                _, point = edit('number café = 1; post(ca@@fé);')
                response = request('rename', point, newName='ação')
                changes = response.get('result', {}).get('changes', {}).get(uri, [])
                assert len(changes) == 2 and all(item['newText'] == 'ação' for item in changes), response
            run('rename permits a valid Unicode identifier', unicode_rename)
            for invalid in ['', 'if', 'two words', 'a.b', '123name', 'next\npost(1);']:
                def invalid_rename(invalid=invalid):
                    _, point = edit(references)
                    response = request('rename', point, newName=invalid)
                    assert response.get('error', {}).get('code') == -32602, response
                run('rename rejects invalid identifier ' + repr(invalid), invalid_rename)

            def stale_change():
                _, point = edit('number current = 1;\npost(cur@@);')
                client.change(uri, 'number obsolete = 2;', version - 1)
                result = request('completion', point)['result']['items']
                assert 'current' in {item['label'] for item in result}, result
            run('out-of-order document versions cannot replace current code', stale_change)

            def incremental():
                nonlocal version
                source = 'post("🍕"); number count = 1; post(count);'
                edit(source)
                changes = []
                for offset in [source.index('count'), source.rindex('count')]:
                    column = len(source[:offset].encode('utf-16-le')) // 2
                    changes.append({'range': {'start': {'line': 0, 'character': column},
                                               'end': {'line': 0, 'character': column + 5}}, 'text': 'score'})
                version += 1
                client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': changes})
                assert not barrier(), client.diags.get(uri)
                response = request('hover', {'line': 0, 'character': changes[1]['range']['start']['character'] + 2})
                assert 'score' in response['result']['contents']['value'], response
            run('incremental edits use UTF16 positions after an emoji', incremental)

            def reversed_edit():
                nonlocal version
                edit('number current = 1; post(current);')
                version += 1
                client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version},
                              'contentChanges': [{'range': {'start': {'line': 0, 'character': 7},
                                                            'end': {'line': 0, 'character': 14}}, 'text': 'corrupt'},
                                                 {'range': {'start': {'line': 0, 'character': 12},
                                                            'end': {'line': 0, 'character': 2}}, 'text': 'broken'}]})
                assert not barrier(), client.diags.get(uri)
                response = request('hover', {'line': 0, 'character': 9})
                assert 'current' in response['result']['contents']['value'], response
            run('reversed edit ranges cannot corrupt the buffer', reversed_edit)

            for method, malformed in [
                ('hover', {'textDocument': {'uri': uri}, 'position': 'bad'}),
                ('rename', {'textDocument': {'uri': uri}, 'position': {'line': 0, 'character': 8}, 'newName': 42}),
                ('completion', {'textDocument': {'uri': 42}, 'position': {'line': 0, 'character': 0}}),
            ]:
                def bad_request(method=method, malformed=malformed):
                    edit('number current = 1; post(current);')
                    response = client.req('textDocument/' + method, malformed)
                    assert response.get('error', {}).get('code') == -32602, response
                    assert request('hover', {'line': 0, 'character': 9}).get('result')
                run('invalid request parameters are diagnosed and recover ' + method, bad_request)

            for text in ['', '<< comentário 🍕', '<<[ block comment ]>>', 'post("🍕");\r\n']:
                def document(text=text):
                    edit(text)
                    diagnostics = barrier()
                    assert not diagnostics, diagnostics
                    tokens = request('semanticTokens/full')['result']['data']
                    assert len(tokens) % 5 == 0, tokens
                run('empty/comment/Unicode document ' + repr(text), document)

            for broken in ['def Broken {', 'post("unterminated', '<<[ unterminated', 'array<array<number> broken;',
                           'void Bad() { post(', 'link "./missing.clp" as Missing;', 'post(unknown);',
                           'post(' + '(' * 300 + '1' + ')' * 300 + ');']:
                def recovery(broken=broken):
                    edit(broken)
                    diagnostics = barrier()
                    assert diagnostics, 'invalid code had no diagnostics'
                    edit('number recovered = 1; post(recovered);')
                    assert not barrier(), 'diagnostics remained after repair'
                run('malformed input repairs without stale diagnostics ' + broken[:45], recovery)

            def cycle():
                (root / 'A.clp').write_text('link "./B.clp" as B; export number A() { return 1; }', encoding='utf8')
                (root / 'B.clp').write_text('link "./A.clp" as A; export number B() { return 2; }', encoding='utf8')
                edit('link "./A.clp" as A;')
                assert barrier(), 'module cycle had no diagnostic'
                (root / 'B.clp').write_text('export number B() { return 2; }', encoding='utf8')
                client.notify('workspace/didChangeWatchedFiles', {'changes': [{'uri': (root / 'B.clp').as_uri(), 'type': 2}]})
                assert not barrier(), client.diags.get(uri)
            run('module cycle diagnosed and repaired through file notification', cycle)

            contract = json.loads((root / 'language/cases.json').read_text(encoding='utf8'))
            for case in contract['positive']:
                def parity(case=case):
                    target = root / 'language' / case['file']
                    text = target.read_text(encoding='utf8')
                    other = client.open(target.as_posix(), text)
                    response = client.req('textDocument/documentSymbol', {'textDocument': {'uri': other}}, timeout=30)
                    assert response and 'result' in response, response
                    assert not client.diags.get(other, []), client.diags.get(other)
                    checked = subprocess.run([str(compiler), '--check', str(target)], capture_output=True, encoding='utf8', timeout=30)
                    assert checked.returncode == 0, checked.stderr
                    client.notify('textDocument/didClose', {'textDocument': {'uri': other}})
                run('CLI/editor acceptance parity ' + case['file'], parity)
            for case in contract['negative']:
                def rejection(case=case):
                    edit(case['source'])
                    assert barrier(), 'editor accepted source rejected by the language contract'
                    compile_source(case['source'], False)
                run('CLI/editor rejection parity ' + case['name'], rejection)

            def cancellation():
                edit('number alive = 1; post(alive);')
                client.notify('$/cancelRequest', {'id': 999999})
                response = request('hover', {'line': 0, 'character': 8})
                assert response.get('result'), response
                response = client.req('$/unknownRequest', {})
                assert response.get('error', {}).get('code') == -32601, response
                assert not barrier(), 'control messages poisoned the document'
            run('late cancellation and unknown control requests leave the server usable', cancellation)

            if args.large:
                def large():
                    text = '\n'.join(f'number value{index} = {index};' for index in range(10000)) + '\npost(value9999);'
                    point = {'line': 10000, 'character': 12}
                    started = time.monotonic()
                    edit(text)
                    response = request('hover', point)
                    cold = (time.monotonic() - started) * 1000
                    assert response.get('result') and cold < 20000, (cold, response)
                    timings = [request('hover', point)['_ms'] for _ in range(20)]
                    print(f'METRIC 10000 bindings cold_ms={cold:.2f} warm_median_ms={statistics.median(timings):.2f} warm_max_ms={max(timings):.2f}', flush=True)
                    # Debug/instrumented correctness smoke: tolerate isolated OS
                    # scheduling pauses, while bounding sustained delay and stalls.
                    # Optimized performance has its own 10k-case p99 <= 50 ms gate.
                    assert statistics.median(timings) < 500 and max(timings) < 2000, timings
                    edit('number small = 1; post(small);')
                    assert not barrier()
                run('10000 declarations and return to small-file editing', large)
        finally:
            client.close()

    suite = ET.Element('testsuite', name='editor-audit', tests=str(len(results)),
                       failures=str(sum(bool(error) for _, _, error in results)), errors='0')
    for name, elapsed, error in results:
        item = ET.SubElement(suite, 'testcase', classname='editor-audit', name=name, time=str(elapsed))
        if error:
            ET.SubElement(item, 'failure', message=error[:400]).text = error
    if args.junit:
        args.junit.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(suite).write(args.junit, encoding='utf-8', xml_declaration=True)
        ET.parse(args.junit)  # Include emoji names in a portable, readable JUnit artifact.
    print(f'{len(results)} checks; {sum(bool(error) for _, _, error in results)} failures')
    return int(any(error for _, _, error in results))


if __name__ == '__main__':
    raise SystemExit(main())
