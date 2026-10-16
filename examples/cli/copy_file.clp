<< A general binary-copy CLI. No dependency on a compiler or project protocol.
link @clpp.fs as Fs;
link @clpp.path as Path;
link @clpp.process as Process;
link @clpp.io as Io;

void Copy(array<string> arguments) {
  if (len(arguments) == 0) {
    Io.Print("copy_file SOURCE DESTINATION (maximum 4 MiB)");
    return;
  }
  if (len(arguments) != 2) { report("expected source and destination paths"); }
  string source = Path.Absolute(arguments[0], "");
  string destination = Path.Absolute(arguments[1], "");
  if (Fs.exists(destination) and Path.Equivalent(source, destination)) {
    report("source and destination refer to the same file");
  }
  array<number> bytes = Fs.ReadBytes(source, 4194304);
  Fs.WriteBytesAtomic(destination, bytes);
  Io.Print("Copied " .: len(bytes) .: " bytes");
}

try { Copy(args()); }
catch (error) { Io.Error("copy_file: " .: error .: "\n"); Process.SetExitCode(1); }
