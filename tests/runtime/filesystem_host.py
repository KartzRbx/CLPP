"""Generic filesystem regression checks for any CLI application."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
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
    with tempfile.TemporaryDirectory(prefix='clpp-filesystem-') as directory:
        root = Path(directory)
        def case(name, callback):
            started = time.monotonic()
            try:
                callback()
                error = ''
            except Exception as failure:
                error = str(failure) or type(failure).__name__
            results.append((name, time.monotonic() - started, error))
            print(('FAIL' if error else 'PASS'), name, error[:1200], flush=True)

        def filesystem():
            script = root / 'fs.clp'
            script.write_text('link @clpp.fs as Fs;\nlink @clpp.path as Path;\nstring directory = Path.Join(Path.CurrentDirectory(), "pasta 漢字");\nif (!Fs.makeDir(directory) or !Fs.isDir(directory)) { report("directory"); }\nstring file = Path.Join(directory, "ação.txt");\nif (!Fs.write(file, "ação") or !Fs.append(file, "!")) { report("write"); }\nif (!Fs.exists(file) or Fs.read(file) != "ação!" or Fs.size(file) != 7) { report("read bytes"); }\narray<string> names = Fs.list(directory);\nif (len(names) != 1 or names[0] != "ação.txt") { report("list encoding"); }\nif (!Fs.remove(file) or !Fs.remove(directory)) { report("remove"); }\npost("filesystem passed");\n', encoding='utf8', newline='\n')
            result = subprocess.run([str(compiler), str(script)], cwd=root, capture_output=True, text=True, encoding='utf8', timeout=30)
            assert result.returncode == 0 and result.stdout.strip() == 'filesystem passed', result.stderr
        case('Unicode file operations', filesystem)

        def nul_paths():
            script = root / 'invalid_paths.clp'
            script.write_text('link @clpp.fs as Fs;\nstring invalid = "protected.txt\\0suffix";\nnumber rejected = 0;\ntry { Fs.write(invalid, "overwrite"); } catch { rejected++; }\ntry { Fs.append(invalid, "overwrite"); } catch { rejected++; }\ntry { Fs.remove(invalid); } catch { rejected++; }\ntry { Fs.exists(invalid); } catch { rejected++; }\ntry { Fs.isDir(invalid); } catch { rejected++; }\ntry { Fs.makeDir(invalid); } catch { rejected++; }\ntry { Fs.read(invalid); } catch { rejected++; }\ntry { Fs.list(invalid); } catch { rejected++; }\npost(rejected); post(Fs.size(invalid));\n', encoding='utf8')
            protected = root / 'protected.txt'
            protected.write_text('unchanged', encoding='utf8')
            result = subprocess.run([str(compiler), str(script)], cwd=root, capture_output=True, text=True, encoding='utf8', timeout=30)
            assert result.returncode == 0 and result.stdout.splitlines() == ['8', '-1'], (result.returncode, result.stdout, result.stderr)
            assert protected.read_text(encoding='utf8') == 'unchanged'
        case('NUL path rejection', nul_paths)

    suite = ET.Element('testsuite', name='cli-filesystem', tests=str(len(results)), failures=str(sum(bool(error) for _, _, error in results)))
    for name, duration, error in results:
        item = ET.SubElement(suite, 'testcase', name=name, time=f'{duration:.6f}')
        if error:
            ET.SubElement(item, 'failure', message=error[:300]).text = error
    if args.junit:
        args.junit.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(suite).write(args.junit, encoding='utf8', xml_declaration=True)
    return 1 if any(error for _, _, error in results) else 0


if __name__ == '__main__':
    raise SystemExit(main())
