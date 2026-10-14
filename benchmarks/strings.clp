<< Concatenação e templates: 100.000 strings
auto n = 0;
for (auto i = 0; i < 100000; i += 1) {
  const auto s = `item ${i}` .: "!";
  n += 1;
}
post(n);
