"""Generic CLI storage/path contracts against the real VM and real temporary files."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
from xml.etree import ElementTree as ET


def main():
    sys.stdout.reconfigure(encoding='utf8')
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path)
    parser.add_argument('--junit', type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    results = []
    with tempfile.TemporaryDirectory(prefix='clpp-cli-storage-') as folder:
        root = Path(folder)
        script = root / 'storage.clp'
        imports = 'link @clpp.fs as Fs; link @clpp.path as Path; link @clpp.process as Process; link @clpp.json as Json; link @clpp.text as Text;\n'

        def run(code, arguments=()):
            script.write_text(imports + code, encoding='utf8', newline='\n')
            result = subprocess.run([str(compiler), str(script), *map(str, arguments)], cwd=root,
                                    capture_output=True, text=True, encoding='utf8', timeout=30)
            assert result.returncode == 0, (result.returncode, result.stderr)
            return result.stdout.splitlines()

        def case(name, function):
            started = time.monotonic()
            try:
                function()
                error = ''
            except Exception as failure:
                error = str(failure) or type(failure).__name__
            results.append((name, time.monotonic() - started, error))
            print('FAIL' if error else 'PASS', name, error[:1200], flush=True)

        def reads():
            (root / 'empty.txt').write_bytes(b'')
            (root / 'text.txt').write_bytes('ação\r\n'.encode('utf8'))
            output = run('''post(Fs.ReadText("empty.txt", 1) == "");
post(Fs.ReadText("text.txt", 8) == "ação\\r\\n");
number caught = 0;
try { Fs.ReadText("missing.txt", 20); } catch { caught++; }
try { Fs.ReadText("text.txt", 7); } catch { caught++; }
try { Fs.ReadText("text.txt", 0); } catch { caught++; }
try { Fs.ReadText("text.txt", 1.5); } catch { caught++; }
post(caught);
''')
            assert output == ['true', 'true', '4'], output
        case('bounded text reads distinguish empty missing oversized and invalid budget', reads)

        def binary():
            original = bytes(range(256))
            (root / 'bytes.bin').write_bytes(original)
            output = run('''array<number> bytes = Fs.ReadBytes("bytes.bin", 256);
post(len(bytes)); post(bytes[0]); post(bytes[255]);
Fs.WriteBytesAtomic("copied.bin", bytes);
Fs.WriteBytesAtomic("empty.bin", array<number>());
''')
            assert output == ['256', '0', '255'], output
            assert (root / 'copied.bin').read_bytes() == original
            assert (root / 'empty.bin').read_bytes() == b''
        case('binary round trip retains every byte including NUL and invalid UTF8', binary)

        def atomic_text():
            (root / 'ação 漢字').mkdir()
            output = run('''Fs.WriteAtomic("ação 漢字/result.txt", "old");
Fs.WriteAtomic("ação 漢字/result.txt", "new\\0ação");
post(Fs.ReadText("ação 漢字/result.txt", 20) == "new\\0ação");
''')
            assert output == ['true'], output
            assert (root / 'ação 漢字/result.txt').read_bytes() == b'new\0' + 'ação'.encode('utf8')
            assert not list(root.rglob('*.clpp-*'))
        case('atomic text replacement supports Unicode paths NUL payload and cleans temporary files', atomic_text)

        def failure_preserves():
            (root / 'preserved.bin').write_bytes(b'original')
            (root / 'directory').mkdir()
            output = run('''number rejected = 0;
try { Fs.WriteBytesAtomic("preserved.bin", array<number>(256)); } catch { rejected++; }
try { Fs.WriteBytesAtomic("preserved.bin", array<number>(-1)); } catch { rejected++; }
try { Fs.WriteBytesAtomic("preserved.bin", array<number>(1.5)); } catch { rejected++; }
try { Fs.WriteAtomic("directory", "bad"); } catch { rejected++; }
try { Fs.WriteAtomic("preserved.bin\\0suffix", "bad"); } catch { rejected++; }
try { Fs.ReadBytes("preserved.bin", 4194305); } catch { rejected++; }
try { Fs.WriteAtomic("absent/result", "bad"); } catch { rejected++; }
post(rejected);
''')
            assert output == ['7'], output
            assert (root / 'preserved.bin').read_bytes() == b'original'
            assert (root / 'directory').is_dir()
            assert not list(root.rglob('*.clpp-*'))
        case('invalid writes preserve destination and failed replacement removes temporary file', failure_preserves)

        def metadata():
            path = root / 'metadata.bin'
            path.write_bytes(b'12345')
            stamp = 1700000000123456789
            os.utime(path, ns=(stamp, stamp))
            values = run('''Fs.FileInfo file = Fs.Stat("metadata.bin");
post(Json.Stringify(file)); post(Json.Stringify(Fs.Stat("not-present"))); post(Json.Stringify(Fs.Stat(".")));
''')
            file, missing, directory = map(json.loads, values)
            assert file[:4] == [True, False, True, 5], file
            assert abs(file[4] - stamp // 1000000) <= 1, file
            assert missing == [False, False, False, 0, 0], missing
            assert directory[:4] == [True, True, False, 0], directory
        case('typed file metadata uses byte size and Unix milliseconds', metadata)

        def path_identity():
            first = root / 'first.txt'
            first.write_bytes(b'first')
            os.link(first, root / 'alias.txt')
            (root / 'different.txt').write_bytes(b'first')
            output = run('''post(Path.Equivalent("first.txt", "alias.txt"));
post(Path.Equivalent("first.txt", "different.txt"));
post(Path.Canonical("directory/../first.txt") == Path.Absolute("first.txt", ""));
number caught=0;
try { Path.Equivalent("first.txt", "absent"); } catch { caught++; }
try { Path.Canonical("bad\\0suffix"); } catch { caught++; }
post(caught);
Fs.WriteAtomic("alias.txt", "replacement");
post(Path.Equivalent("first.txt", "alias.txt"));
''')
            assert output == ['true', 'false', 'true', '2', 'false'], output
            assert first.read_bytes() == b'first'
            assert (root / 'alias.txt').read_bytes() == b'replacement'
        case('path identity detects hard links and atomic replacement preserves their other names', path_identity)

        def real_workspace_path():
            repo = Path(__file__).resolve().parents[2]
            output = run('''string base = args()[0];
post(Path.Equivalent(Path.Canonical(base), base));
post(Path.IsAbsolute(Path.Canonical(Path.Join(base, "uncreated-directory/child.txt"))));
''', [repo])
            assert output == ['true', 'true'], output
        case('canonical paths support the actual checkout including cloud-managed directories', real_workspace_path)

        def executable():
            output = run('post(Path.Equivalent(Process.ExecutablePath(), args()[0]));', [compiler])
            assert output == ['true'], output
        case('executable discovery identifies actual VM independently of cwd', executable)

        def cli_examples():
            repo = Path(__file__).resolve().parents[2]
            data = bytes(range(256))
            source = root / 'cli-source.bin'
            target = root / 'cli-output.bin'
            source.write_bytes(data)
            copy = repo / 'examples/cli/copy_file.clp'
            result = subprocess.run([str(compiler), str(copy), str(source), str(target)],
                                    capture_output=True, text=True, encoding='utf8', timeout=30)
            assert result.returncode == 0 and target.read_bytes() == data, result.stderr
            alias = root / 'cli-alias.bin'
            os.link(source, alias)
            result = subprocess.run([str(compiler), str(copy), str(source), str(alias)],
                                    capture_output=True, text=True, encoding='utf8', timeout=30)
            assert result.returncode == 1 and 'same file' in result.stderr, result.stderr
            assert source.read_bytes() == data
            inspect = repo / 'examples/cli/file_info.clp'
            result = subprocess.run([str(compiler), str(inspect), str(source)],
                                    capture_output=True, text=True, encoding='utf8', timeout=30)
            assert result.returncode == 0, result.stderr
            info = json.loads(result.stdout)
            assert info['exists'] is True and info['bytes'] == 256 and info['directory'] is False, info
        case('general CLI examples copy binary reject aliases and report named JSON metadata', cli_examples)

        def atomic_observers():
            target = root / 'observed.txt'
            choices = {b'A' * 65536, b'B' * 65536}
            target.write_bytes(next(iter(choices)))
            stopped = threading.Event()
            failures = []
            observations = []
            # Windows readers must permit deletion/replacement of their open handle.
            if os.name == 'nt':
                kernel = ctypes.WinDLL('kernel32', use_last_error=True)
                kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
                                              ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
                kernel.CreateFileW.restype = ctypes.c_void_p
                kernel.ReadFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32,
                                           ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p]
                kernel.CloseHandle.argtypes = [ctypes.c_void_p]

            def read_shared():
                if os.name != 'nt':
                    return target.read_bytes()
                handle = kernel.CreateFileW(str(target), 0x80000000, 7, None, 3, 0, None)
                if handle == ctypes.c_void_p(-1).value:
                    raise OSError(ctypes.get_last_error(), 'shared read open failed')
                try:
                    buffer = ctypes.create_string_buffer(65537)
                    count = ctypes.c_uint32()
                    if not kernel.ReadFile(handle, buffer, len(buffer), ctypes.byref(count), None):
                        raise OSError(ctypes.get_last_error(), 'shared read failed')
                    return buffer.raw[:count.value]
                finally:
                    kernel.CloseHandle(handle)

            def observe():
                try:
                    while not stopped.is_set():
                        value = read_shared()
                        if value not in choices:
                            failures.append(f'partial output length {len(value)}')
                            break
                        observations.append(1)
                        time.sleep(0.001)
                except Exception as error:
                    failures.append(str(error))

            observer = threading.Thread(target=observe)
            observer.start()
            try:
                run('''string first = Text.repeat("A",65536); string second = Text.repeat("B",65536);
for (number i=0; i<80; i++) { Fs.WriteAtomic("observed.txt", i % 2 == 0 ? first : second); }
''')
            finally:
                stopped.set()
                observer.join(timeout=5)
            assert not observer.is_alive() and not failures and observations, (failures, len(observations))
        case('concurrent readers observe complete old or new contents', atomic_observers)

    suite = ET.Element('testsuite', name='generic-cli-storage', tests=str(len(results)),
                       failures=str(sum(bool(error) for _, _, error in results)))
    for name, elapsed, error in results:
        item = ET.SubElement(suite, 'testcase', name=name, time=str(elapsed))
        if error:
            ET.SubElement(item, 'failure', message=error[:500]).text = error
    if args.junit:
        args.junit.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(suite).write(args.junit, encoding='utf8', xml_declaration=True)
    return int(any(error for _, _, error in results))


if __name__ == '__main__':
    raise SystemExit(main())
