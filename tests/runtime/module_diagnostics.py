"""Regression: runtime errors must identify the imported source, not the entry file."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    with tempfile.TemporaryDirectory(prefix='clpp-module-diagnostics-') as temporary:
        root = Path(temporary) / 'project ação'
        library = root / 'lib'
        library.mkdir(parents=True)
        entry = root / 'main.clp'
        broken = library / 'broken.clp'
        broken.write_text('export void Fail() {\n' + '\n' * 6 + '  report("failure inside module");\n}\n', encoding='utf8')
        entry.write_text('link "./lib/broken.clp" as Broken;\nBroken.Fail();\n', encoding='utf8')
        result = subprocess.run([str(compiler), str(entry)], capture_output=True, text=True, encoding='utf8', timeout=15)
        assert result.returncode == 1, result
        expected = f'{broken}:8:'
        assert expected in result.stderr, (expected, result.stderr)
        assert f'{entry}:8:' not in result.stderr, result.stderr
        assert 'runtime error: failure inside module' in result.stderr, result.stderr
        print('PASS imported runtime report uses module path and line')
        wrapper = library / 'wrapper.clp'
        wrapper.write_text('link "./broken.clp" as Broken;\nexport void Run() { Broken.Fail(); }', encoding='utf8')
        for name, code in [
            ('nested module', 'link "./lib/wrapper.clp" as Wrapper;\nWrapper.Run();'),
            ('joined worker', 'link "./lib/broken.clp" as Broken;\nauto worker = thread Broken.Fail(); join(worker);'),
            ('unobserved worker', 'link "./lib/broken.clp" as Broken;\nauto worker = thread Broken.Fail();'),
        ]:
            entry.write_text(code, encoding='utf8')
            result = subprocess.run([str(compiler), str(entry)], capture_output=True, text=True, encoding='utf8', timeout=15)
            assert result.returncode == 1 and expected in result.stderr, (name, result.returncode, result.stderr)
            print('PASS', name, 'preserves failing module origin')
        unicode_module = library / 'bibliotéca' / 'erro漢.clp'
        unicode_module.parent.mkdir()
        unicode_module.write_bytes(broken.read_bytes())
        entry.write_text('link "./lib/bibliotéca/erro漢.clp" as Broken;\nBroken.Fail();', encoding='utf8')
        result = subprocess.run([str(compiler), str(entry)], capture_output=True, text=True, encoding='utf8', timeout=15)
        assert result.returncode == 1 and f'{unicode_module}:8:' in result.stderr, result.stderr
        print('PASS Unicode module filename and directory preserve source origin')
        entry.write_text('number value = "bad";', encoding='utf8')
        result = subprocess.run([str(compiler), '--check', str(entry)], capture_output=True, text=True, encoding='utf8', timeout=15)
        assert result.returncode == 1 and 'expected number, received "bad"' in result.stderr, result.stderr
        print('PASS CLI type diagnostic includes expected and received type')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
