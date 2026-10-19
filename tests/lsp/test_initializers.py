"""Initializer field completion and ownership over the real editor protocol."""
from pathlib import Path
import sys
import tempfile
from lsp_client import Client

exe = str(Path(sys.argv[1]).resolve())
checks = 0


def require(condition, label, detail=None):
    global checks
    checks += 1
    assert condition, (label, detail)
    print('PASS', label)


with tempfile.TemporaryDirectory(prefix='clpp-initializers-') as directory:
    root = Path(directory)
    module = ('export def Human {\n  string name;\n  number age;\n  optional<number> cpf;\n'
              '  private number secret = 7;\n  void Speak() {}\n}\n'
              'export def Base<T> { T inherited; }\n'
              'export def Box<T> : Base<T> { T value; array<T> items; }\n'
              'export def Alias<T> = Box<T>;\n'
              'export def Nested { Human person; number count; }\n')
    (root / 'Human.clp').write_text(module, encoding='utf8')
    client = Client(exe, root.as_posix())
    uri = client.open((root / 'main.clp').as_posix(), '')
    version = 1

    def edit(marked):
        global version
        offset = marked.index('|')
        source = marked[:offset] + marked[offset + 1:]
        prefix = source[:offset]
        line = prefix.count('\n')
        column = len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2
        version += 1
        client.change(uri, source, version)
        return source, line, column

    def complete(marked):
        source, line, column = edit('link "./Human.clp" as H;\n' + marked)
        response = client.pos(uri, 'textDocument/completion', line, column)
        require(response is not None, 'completion returns within deadline')
        result = response.get('result') or []
        items = result.get('items', []) if isinstance(result, dict) else result
        return {item['label']: item for item in items if item.get('kind') == 5}

    try:
        triggers = client.caps['completionProvider']['triggerCharacters']
        require('{' in triggers and ',' in triggers, 'initializer delimiters trigger completion')
        fields = complete('auto kartz = H.Human { | };')
        require(set(fields) == {'name', 'age', 'cpf'}, 'imported definition offers public fields only', fields)
        require(fields['name']['detail'] == 'string', 'name has its declared type', fields)
        require(fields['cpf']['detail'] == 'optional<number>', 'optional field retains its type', fields)
        fields = complete('auto kartz = H.Human { na| };')
        require(set(fields) == {'name'}, 'unfinished field prefix resolves to name', fields)
        fields = complete('auto kartz = H.Human { name = "Kartz", | };')
        require(set(fields) == {'age', 'cpf'}, 'already supplied fields are omitted', fields)
        fields = complete('auto kartz = H.Human { na|me = "Kartz", age = 18 };')
        require(set(fields) == {'name'}, 'editing an existing key retains that field', fields)
        fields = complete('auto box = H.Box<string> { | };')
        require(set(fields) == {'inherited', 'value', 'items'}, 'generic constructor includes inherited fields', fields)
        require(fields['inherited']['detail'] == 'string' and fields['value']['detail'] == 'string' and
                fields['items']['detail'] == 'array<string>', 'generic field types specialize recursively', fields)
        fields = complete('auto box = H.Alias<number> { | };')
        require(fields.get('value', {}).get('detail') == 'number', 'generic aliases use semantic field types', fields)
        fields = complete('auto box = H.Box<array<number>> { | };')
        require(fields.get('items', {}).get('detail') == 'array<array<number>>', 'nested generic arguments stay intact', fields)
        fields = complete('auto box = H.Box<H.Human> { | };')
        require(fields.get('value', {}).get('detail') == 'H.Human', 'imported generic arguments retain their display names', fields)
        fields = complete('string greeting = "Olá 🍕"; auto kartz = H.Human { na| };')
        require(set(fields) == {'name'}, 'UTF16 cursor after Unicode retains field completion', fields)
        fields = complete('def Local { private number hidden = 1; number visible; } auto value = Local { | };')
        require(set(fields) == {'visible'}, 'local private fields stay hidden outside the owner', fields)
        fields = complete('def Local { private number hidden = 1; number visible; void Build() { auto value = Local { | }; } }')
        require(set(fields) == {'visible', 'hidden'}, 'owner methods can initialize their private fields', fields)
        fields = complete('template<typename T> void Build(T input) { auto box = H.Box<T> { | }; }')
        require(fields.get('value', {}).get('detail') == 'T', 'generic functions retain their formal field types', fields)
        fields = complete('auto outer = H.Nested { person = H.Human { na| } };')
        require(set(fields) == {'name'}, 'nested initializer uses the inner definition', fields)
        fields = complete('auto outer = H.Nested { person = H.Human { name = "a", age = 1 }, | };')
        require(set(fields) == {'count'}, 'completion returns to the outer initializer', fields)
        for source in (
            'auto kartz = H.Human { name = va| };',
            'number Read(number a, number b) { return a; } auto kartz = H.Human { age = Read(1, |) };',
            'template<typename T, typename U> number Pick(T a, U b) { return 1; } auto kartz = H.Human { age = Pick<number, | };',
            'auto kartz = H.Human { name = "na|" };',
            'auto kartz = H.Human { << na|\n };',
            'def Local { | };',
        ):
            require(not complete(source), 'field choices do not leak into values, arguments, comments or declarations', source)

        marked = ('link "./Human.clp" as H;\nnumber age = 99;\n'
                  'auto kartz = H.Human { name = "Kartz", ag|e = 18 };\n')
        source, line, column = edit(marked)
        hover = client.pos(uri, 'textDocument/hover', line, column).get('result') or {}
        text = (hover.get('contents') or {}).get('value', '')
        require('H.Human.age' in text and 'number' in text and '99' not in text, 'key hover names its owning field, not a local', text)
        definition = client.pos(uri, 'textDocument/definition', line, column).get('result') or {}
        require(definition.get('uri', '').endswith('/Human.clp') and definition.get('range', {}).get('start') ==
                {'line': 2, 'character': 9}, 'key definition jumps to the original imported field', definition)
        response = client.req('textDocument/semanticTokens/full', {'textDocument': {'uri': uri}})
        legend = client.caps['semanticTokensProvider']['legend']['tokenTypes']
        row = character = 0
        roles = {}
        data = response.get('result', {}).get('data', [])
        for index in range(0, len(data), 5):
            dr, dc, length, role, modifiers = data[index:index + 5]
            row += dr
            character = dc if dr else character + dc
            roles[(row, character, length)] = legend[role]
        require(roles.get((line, column - 2, 3)) == 'property', 'initializer key has a property semantic role', roles)

        source, line, column = edit('link "./Human.clp" as H;\nauto box = H.Box<string> { inhe|rited = "x", value = "v", items = array<string>() };')
        hover = client.pos(uri, 'textDocument/hover', line, column).get('result') or {}
        text = (hover.get('contents') or {}).get('value', '')
        require('string' in text and 'H.Base.inherited' in text, 'inherited key hover shows concrete type and declaring owner', text)
        definition = client.pos(uri, 'textDocument/definition', line, column).get('result') or {}
        require(definition.get('uri', '').endswith('/Human.clp') and definition.get('range', {}).get('start', {}).get('line') == 7,
                'inherited key definition goes to the base declaration', definition)
    finally:
        client.close()

print(f'PASS {checks} initializer checks')
