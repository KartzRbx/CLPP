"""Real child-process contract tests; no shell, network, or external tool installation."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from xml.etree import ElementTree as ET


def literal(text):
    return json.dumps(str(text), ensure_ascii=False)


def main():
    sys.stdout.reconfigure(encoding='utf8')
    sys.stderr.reconfigure(encoding='utf8')
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--junit', type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    results = []
    with tempfile.TemporaryDirectory(prefix='clpp-process-') as directory:
        root = Path(directory)
        unicode_dir = root / 'ação 漢字'
        unicode_dir.mkdir()
        source = unicode_dir / 'ferramenta.clp'
        child = root / 'child.py'
        child.write_text(r'''import json, os, subprocess, sys, time
mode = sys.argv[1]
if mode == 'stdio':
 data = sys.stdin.buffer.read()
 sys.stdout.buffer.write(data + b'|out'); sys.stdout.buffer.flush()
 sys.stderr.buffer.write(os.environ.get('CLPP_TEST_ENV','absent').encode('utf8') + b'|err')
 sys.exit(17)
if mode == 'args':
 sys.stdout.buffer.write(json.dumps(sys.argv[2:],ensure_ascii=False).encode('utf8'))
elif mode == 'environment':
 sys.stdout.buffer.write(json.dumps(dict(value=os.environ.get('CLPP_TEST_ENV'),inherited=os.environ.get('CLPP_TEST_PARENT')),ensure_ascii=False).encode('utf8'))
elif mode == 'timeout':
 print('started',flush=True); time.sleep(20)
elif mode == 'descendant':
 subprocess.Popen([sys.executable,'-c','import time; time.sleep(20)'])
 sys.stdout.buffer.write(b'leader exited\n'); sys.stdout.buffer.flush()
elif mode == 'flood':
 while True: sys.stdout.buffer.write(b'x'*8192); sys.stdout.buffer.flush()
elif mode == 'duplex':
 sys.stdout.buffer.write(b'O'*131072);sys.stdout.buffer.flush()
 sys.stderr.buffer.write(b'E'*131072);sys.stderr.buffer.flush()
 data=sys.stdin.buffer.read();sys.stdout.buffer.write(str(len(data)).encode('ascii'))
elif mode == 'close-input':
 sys.stdout.write('closed');sys.exit(0)
''', encoding='utf8')
        environment = dict(os.environ, CLPP_TEST_PARENT='parent-only')
        imports = 'link @clpp.process as Process; link @clpp.json as Json; link @clpp.path as Path; link @clpp.io as Io;\n'

        def run(code, arguments=(), stdin=''):
            source.write_text(imports + code, encoding='utf8')
            return subprocess.run([str(compiler), str(source), *arguments], cwd=root,
                                  env=environment, input=stdin, capture_output=True, text=True,
                                  encoding='utf8', timeout=20)

        def process(mode, extra=(), options=''):
            values = ', '.join(literal(x) for x in [child, mode, *extra])
            code = f'Process.Result result = Process.Run({literal(sys.executable)}, array<string>({values}), Process.Options {{ {options} }});\npost(Json.Stringify(result));\n'
            result = run(code)
            assert result.returncode == 0, (result.returncode, result.stderr)
            parsed = json.loads(result.stdout)
            assert len(parsed) == 5, parsed
            return parsed

        def case(name, callback):
            started = time.monotonic()
            try:
                callback()
                results.append((name, time.monotonic() - started, ''))
                print('PASS', name, flush=True)
            except Exception as failure:
                message = str(failure) or type(failure).__name__
                results.append((name, time.monotonic() - started, message))
                print('FAIL', name, message[:1000], flush=True)

        def stdio():
            result = process('stdio', options='input = "entrada ação", environment = array<Process.EnvironmentEntry>(Process.EnvironmentEntry { name = "CLPP_TEST_ENV", value = "válido" })')
            assert result == [17, 'entrada ação|out', 'válido|err', False, False], result
        case('separate stdout stderr stdin Unicode and nonzero child exit', stdio)

        def argv():
            values = ['ação 漢字', '', 'space here', 'a"b', '\\tail\\', '& echo injected > sentinel.txt', '$(echo nope)']
            result = process('args', values)
            assert result[0] == 0 and json.loads(result[1]) == values, result
            assert not (root / 'sentinel.txt').exists()
        case('argument boundaries and shell metacharacters remain literal', argv)

        def isolated_environment():
            result = process('environment', options='inheritEnvironment = false, environment = array<Process.EnvironmentEntry>(Process.EnvironmentEntry { name = "CLPP_TEST_ENV", value = "child-only" })')
            assert result[0] == 0 and json.loads(result[1]) == {'value': 'child-only', 'inherited': None}, result
        case('explicit environment without parent inheritance', isolated_environment)

        def timeout():
            started = time.monotonic()
            result = process('timeout', options='timeoutMs = 500')
            assert result[0] == -1 and result[3] and not result[4], result
            assert time.monotonic() - started < 10
        case('timeout stops the owned child', timeout)

        def descendants():
            started = time.monotonic()
            result = process('descendant')
            assert result[0] == 0 and result[1] == 'leader exited\n' and not result[3], result
            assert time.monotonic() - started < 10
        case('leader exit cleans descendants holding inherited pipes', descendants)

        def limited():
            result = process('flood', options='maxOutputBytes = 1024, timeoutMs = 5000')
            assert result[0] == -1 and result[4] and len(result[1]) <= 1024, result[:1]
        case('output limit is bounded and explicitly reported', limited)

        def duplex():
            result = process('duplex', options='input = ' + literal('I' * 262144))
            assert result == [0, 'O' * 131072 + '262144', 'E' * 131072, False, False]
        case('large simultaneous output stderr and input avoid pipe deadlock', duplex)

        def early_close():
            result = process('close-input', options='input = ' + literal('I' * 262144))
            assert result == [0, 'closed', '', False, False], result
        case('child closing stdin does not crash the host', early_close)

        def paths_and_args():
            result = run('post(Path.Normalize("a/../b")); post(Path.Join("x", "../y"));\n'
                         'post(Path.FileName("dir/ação.clp")); post(Path.IsAbsolute(Path.Absolute("child", Path.CurrentDirectory())));\n'
                         'post(len(Path.TempDirectory()) > 0); post(Json.Stringify(args()));\n', ['ação 漢字', '', 'a b'])
            assert result.returncode == 0 and result.stdout.splitlines()[:5] == ['b','y','ação.clp','true','true'], result.stderr
            assert json.loads(result.stdout.splitlines()[5]) == ['ação 漢字', '', 'a b']
        case('Unicode script path arguments and lexical path API', paths_and_args)

        def status_and_errors():
            result = run('Io.Error("diagnóstico\\n"); post("done"); Process.SetExitCode(23);')
            assert result.returncode == 23 and result.stdout == 'done\n' and result.stderr == 'diagnóstico\n', result
            result = run('Process.SetExitCode(23); report("original failure");')
            assert result.returncode == 1 and 'original failure' in result.stderr, result
        case('controlled parent exit code and independent stderr preserve failures', status_and_errors)

        def invalid_options():
            for options in ['timeoutMs = 0', 'timeoutMs = 9223372036854775807', 'maxOutputBytes = 0']:
                result = run(f'auto result = Process.Run({literal(sys.executable)}, array<string>(), Process.Options {{ {options} }});')
                assert result.returncode == 1 and 'invalid Options' in result.stderr, (options, result.stderr)
            result = run(f'auto result = Process.Run({literal(root / "missing.exe")}, array<string>(), Process.Options {{}});')
            assert result.returncode == 1 and 'executable path does not exist' in result.stderr, result
            result = run('Process.SetExitCode(256);')
            assert result.returncode == 1 and '0..255' in result.stderr, result
        case('invalid limits missing executable and exit code fail without spawning', invalid_options)

    failed = sum(bool(error) for _, _, error in results)
    if args.junit:
        suite = ET.Element('testsuite', name='CLI host primitives', tests=str(len(results)), failures=str(failed), errors='0')
        for name, elapsed, error in results:
            test = ET.SubElement(suite, 'testcase', name=name, time=str(elapsed))
            if error: ET.SubElement(test, 'failure', message=error[:1000]).text = error
        ET.ElementTree(suite).write(args.junit, encoding='utf8', xml_declaration=True)
    print(f'{len(results)} cases, {failed} failures')
    return int(failed != 0)


if __name__ == '__main__':
    raise SystemExit(main())
