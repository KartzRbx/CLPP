<< Algorithm optimization: O(n*q) scanning versus O(n+q) prefix preprocessing.
<< Count additions instead of claiming a machine-specific timing improvement.
array<number> input = { 2, 4, 6, 8, 10, 12, 14, 16 };
array<number> prefix = { 0 };
number buildAdds = 0;
for (number value in input) {
  push(prefix, prefix[len(prefix) - 1] + value);
  buildAdds += 1;
}
array<number> starts = { 0, 1, 2, 0 };
array<number> ends = { 8, 7, 6, 4 }; << Half-open intervals.
number scanAdds = 0;
for (number query in len(starts)) {
  number total = 0;
  number index = starts[query];
  while (index < ends[query]) { total += input[index]; index += 1; scanAdds += 1; }
  number cached = prefix[ends[query]] - prefix[starts[query]];
  if (total != cached) { report("prefix result mismatch"); }
  post(cached);
}
post("scan additions:" .: scanAdds);
post("prefix additions:" .: buildAdds);
post("query subtractions:" .: len(starts));
