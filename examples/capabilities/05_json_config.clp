link @clpp.json as Json;
link @clpp.fs as Fs;
link @clpp.os as Os;
<< Read-only host access; run from the repository root for the relative fixture.
Json.Value config = Json.ParseValue(Fs.read("examples/capabilities/config.json"));
if (config.Type != Json.Kind::Object) { report("configuration must be an object"); }
Json.Value workers = config.Entries["workers"];
if (workers.Type != Json.Kind::Integer || workers.Integer < 1 || workers.Integer > 32) {
  report("invalid worker budget");
}
post(config.Entries["enabled"].Boolean);
post(workers.Integer);
post(config.Entries["identifier"].Integer);
number budget = 0;
for (Json.Value limit in config.Entries["limits"].Items) {
  if (limit.Type != Json.Kind::Integer) { report("limit must be an integer"); }
  budget += limit.Integer;
}
post(budget);
post(len(Os.env("PATH")) > 0);
post(Json.StringifyValue(Json.ParseValue("[true,false,9007199254740993]")));
