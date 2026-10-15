<< A general CLI that writes structured file metadata to stdout.
link @clpp.fs as Fs;
link @clpp.path as Path;
link @clpp.process as Process;
link @clpp.json as Json;
link @clpp.io as Io;

void Inspect(array<string> arguments) {
  if (len(arguments) == 0) { Io.Print("file_info PATH"); return; }
  if (len(arguments) != 1) { report("expected exactly one path"); }
  string path = Path.Absolute(arguments[0], "");
  Fs.FileInfo info = Fs.Stat(path);
  Json.Value output = Json.Value { Type = Json.Kind::Object };
  output.Entries["path"] = Json.Value { Type = Json.Kind::Text, Text = path };
  output.Entries["exists"] = Json.Value { Type = Json.Kind::Boolean, Boolean = info.exists };
  output.Entries["directory"] = Json.Value { Type = Json.Kind::Boolean, Boolean = info.isDirectory };
  output.Entries["bytes"] = Json.Value { Type = Json.Kind::Integer, Integer = info.size };
  output.Entries["modifiedUnixMs"] = Json.Value { Type = Json.Kind::Integer, Integer = info.modifiedUnixMs };
  Io.Print(Json.StringifyValue(output));
}

try { Inspect(args()); }
catch (error) { Io.Error("file_info: " .: error .: "\n"); Process.SetExitCode(1); }
