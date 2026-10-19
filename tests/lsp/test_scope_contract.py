"""Validate :: execution, rejection and editor completion against a real compiler."""
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
    compiler = args.compiler.resolve()
    results = []

    def run(name, check):
        try:
            check()
            error = ''
        except Exception as failure:
            error = str(failure) or type(failure).__name__
        results.append((name, error))
        print(('FAIL ' if error else 'PASS ') + name + (' ' + error[:1800] if error else ''), flush=True)

    with tempfile.TemporaryDirectory(prefix='clpp-scope-contract-') as folder:
        root = Path(folder)
        module = ('export def Base { number inherited = 7; private number secret = 1; '
                  'number Read() { return @this.inherited; } }\n'
                  'export def Player : Base { string name = "Kartz"; '
                  'signal<number> Joined = signal<number>(); }\n'
                  'export def Box<T> { T value; T Echo(T input) { return input; } }\n'
                  'export def TextBox : Box<string> {}\n'
                  'export enum class Color { Red, Green }\n')
        (root/'Model.clp').write_text(module, encoding='utf8')
        program = root/'main.clp'

        def execution(source, expected=None, reject=False):
            program.write_text(source, encoding='utf8')
            checked = subprocess.run([str(compiler), '--check', str(program)], capture_output=True,
                                     text=True, encoding='utf8', timeout=20)
            assert checked.returncode == (1 if reject else 0), (source, checked.stdout, checked.stderr)
            if reject:
                assert 'error:' in checked.stderr, checked.stderr
                return
            actual = subprocess.run([str(compiler), str(program)], capture_output=True,
                                    text=True, encoding='utf8', timeout=20)
            assert actual.returncode == 0, (source, actual.stderr)
            assert actual.stdout.splitlines() == expected, (source, actual.stdout, expected)

        positive = [
            ('Connect Fire Disconnect Connected', 'signal<number> event; number total = 0; '
             'connection handle = event::Connect(func(number value) -> void { total += value; }); '
             'event::Fire(4); post(total); post(handle::Connected()); handle::Disconnect(); '
             'handle::Disconnect(); event::Fire(8); post(total); post(handle.Connected);', ['4','true','4','false']),
            ('Once runs exactly once', 'signal<number> event; number total = 0; '
             'auto handle = event::Once(func(number value) -> void { total += value; }); '
             'event::Fire(2); event::Fire(9); post(total); post(handle::Connected());', ['2','false']),
            ('Clear releases managed subscriptions only', 'signal<number> event; number total = 0; '
             'auto regular = event::Connect(func(number value) -> void { total += value; }); '
             'auto managed = event~>Connect(func(number value) -> void { total += 10 * value; }); '
             'event::Fire(1); event::Clear(); event::Clear(); event::Fire(1); '
             'post(total); post(regular.Connected); post(managed.Connected);', ['12','true','false']),
            ('Wait resumes typed payload', 'async number Read(signal<number> event) { return event::Wait(); } '
             'signal<number> event; auto pending = spawn Read(event); event::Fire(42); post(await pending);', ['42']),
            ('Wait returns a typed tuple', 'async number Read(signal<number,string> event) { '
             'tuple<number,string> data = event::Wait(); post(data[1]); return data[0]; } '
             'signal<number,string> event; auto pending = spawn Read(event); event::Fire(7,"ok"); post(await pending);', ['ok','7']),
            ('empty payload Fire', 'signal<> event; number total = 0; '
             'auto handle = event::Connect(func() -> void { total += 1; }); event::Fire(); post(total);', ['1']),
            ('enum scope access', 'enum class Direction { Left, Right } '
             'Direction facing = Direction::Left; post(facing == Direction::Left);', ['true']),
            ('imported enum scope access', 'link "./Model.clp" as M; post(M.Color::Red == M.Color::Red);', ['true']),
            ('external method declaration and receiver', 'def Point { number X; number Y; } '
             'number Point::Sum() { return @this.X + @this.Y; } '
             'Point point = Point { X = 3, Y = 4 }; post(point.Sum());', ['7']),
            ('signal in an inherited object', 'link "./Model.clp" as M; M.Player player = M.Player {}; '
             'auto handle = player.Joined::Connect(func(number value) -> void { post(value); }); '
             'player.Joined::Fire(5); post(player.Read());', ['5','7']),
            ('signal returned by function', 'signal<number> event; signal<number> Get() { return event; } '
             'auto handle = Get()::Connect(func(number value) -> void { post(value); }); Get()::Fire(6);', ['6']),
            ('generic signal parameter', 'template<typename T> void Emit(signal<T> event, T value) { event::Fire(value); } '
             'signal<number> event; auto handle = event::Connect(func(number value) -> void { post(value); }); '
             'Emit<number>(event, 9);', ['9']),
        ]
        for name, source, expected in positive:
            run('runtime ' + name, lambda source=source, expected=expected: execution(source, expected))
        negative = [
            ('wrong signal payload', 'signal<number> event; event::Fire("bad");'),
            ('wrong callback signature', 'signal<number> event; event::Connect(func(string value) -> void {});'),
            ('unknown event operation', 'signal<number> event; event::Missing();'),
            ('connection-only method on signal', 'signal<number> event; event::Disconnect();'),
            ('Wait outside async', 'signal<number> event; event::Wait();'),
            ('unknown enum case', 'enum class Direction { Left } post(Direction::Missing);'),
            ('triple colon rejected', 'signal<number> event; event:::Fire(1);'),
            ('single colon cannot directly call Connect', 'signal<number> event; event:Connect(func(number value) -> void {});'),
        ]
        for name, source in negative:
            run('reject ' + name, lambda source=source: execution(source, reject=True))

        client = Client(str(compiler), root.as_posix())
        uri = client.open(program.as_posix(), '')
        version = 1

        def complete(marked, expected, excluded=(), details=None, accept=False):
            nonlocal version
            offset = marked.index('@@')
            before = marked[:offset]
            source = marked.replace('@@','')
            line = before.count('\n')
            column = len(before.rsplit('\n',1)[-1].encode('utf-16-le'))//2
            version += 1
            client.change(uri, source, version)
            response = client.pos(uri, 'textDocument/completion', line, column)
            assert response and 'result' in response, response
            value = response['result']; items = value['items'] if isinstance(value,dict) else value
            labels = {item['label'] for item in items}
            assert set(expected) <= labels and not set(excluded) & labels, sorted(labels)
            by_name = {item['label']: item for item in items}
            for label, part in (details or {}).items():
                assert part in by_name[label]['detail'], by_name[label]
            if accept:
                insertion = re.sub(r'\$\{\d+:([^}]*)\}',r'\1',by_name['Connect']['insertText'])
                insertion = re.sub(r'\$\d+','',insertion)
                program.write_text(marked.replace('@@',insertion),encoding='utf8')
                checked = subprocess.run([str(compiler),'--check',str(program)],capture_output=True,text=True,timeout=20)
                assert checked.returncode == 0,(program.read_text(),checked.stderr)

        try:
            methods = ['Connect','Once','Fire','Clear','Wait']
            lsp = [
                ('scoped enum cases', 'enum class Color { Red, Green }\nColor::@@', ['Red','Green'], []),
                ('imported scoped enum cases', 'link "./Model.clp" as M;\nM.Color::@@', ['Red','Green'], []),
                ('inherited fields methods and visibility', 'link "./Model.clp" as M;\nM.Player player = M.Player {};\nplayer.@@', ['name','inherited','Read','Joined'], ['secret']),
                ('initializer own and inherited fields', 'link "./Model.clp" as M;\nM.Player player = M.Player { @@ };', ['name','inherited','Joined'], ['secret','Read']),
                ('callback inherits concrete player members', 'link "./Model.clp" as M;\nsignal<M.Player> event;\nevent::Connect(func(M.Player player) -> void { player.@@ });', ['name','inherited','Read'], ['secret']),
                ('signal field scope methods', 'link "./Model.clp" as M;\nM.Player player = M.Player {};\nplayer.Joined::@@', methods, []),
                ('parenthesized signal scope methods', 'signal<number> event;\n(event)::@@', methods, []),
                ('returned signal scope methods', 'signal<number> event;\nsignal<number> Get() { return event; }\nGet()::@@', methods, []),
                ('generic signal parameter scope methods', 'template<typename T> void Use(signal<T> event) {\nevent::@@\n}', methods, []),
            ]
            for name, source, expected, excluded in lsp:
                run('completion ' + name, lambda source=source, expected=expected, excluded=excluded: complete(source, expected, excluded))
            run('completion field first colon acceptance',lambda: complete(
                'link "./Model.clp" as M;\nM.Player player = M.Player {};\nplayer.Joined:@@;',methods,accept=True))
            run('completion returned signal first colon acceptance',lambda: complete(
                'signal<number> event;\nsignal<number> Get() { return event; }\nGet():@@;',methods,accept=True))
            run('completion parenthesized first colon acceptance',lambda: complete(
                'signal<number> event;\n(event):@@;',methods,accept=True))
            run('completion generic callback acceptance',lambda: complete(
                'template<typename T> void Use(signal<T> event) {\nevent::@@;\n}',methods,accept=True))
            run('completion generic member field type',lambda: complete(
                'link "./Model.clp" as M;\nM.Box<number> box = M.Box<number> { value = 4 };\nbox.@@',
                ['value','Echo'],details={'value':'number'}))
            run('completion generic member method signature',lambda: complete(
                'link "./Model.clp" as M;\nM.Box<number> box = M.Box<number> { value = 4 };\nbox.@@',
                ['value','Echo'],details={'Echo':'number'}))
            run('completion inherited generic member types',lambda: complete(
                'link "./Model.clp" as M;\nM.TextBox box = M.TextBox { value = "text" };\nbox.@@',
                ['value','Echo'],details={'value':'string','Echo':'string'}))
            run('completion indexed signal first colon acceptance',lambda: complete(
                'signal<number> event;\narray<signal<number>> events = { event };\nevents[0]:@@;',methods,accept=True))
        finally:
            client.close()
    if args.junit:
        suite = ET.Element('testsuite', name='scope-contract', tests=str(len(results)), failures=str(sum(bool(e) for _, e in results)))
        for name, error in results:
            case = ET.SubElement(suite, 'testcase', name=name)
            if error: ET.SubElement(case, 'failure', message=error).text=error
        args.junit.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(suite).write(args.junit, encoding='utf8', xml_declaration=True)
    return 1 if any(error for _,error in results) else 0


if __name__=='__main__':
    raise SystemExit(main())
