number count = 0;
while (count < 2) { count++; }
do { count--; } while (count > 0);
number total = 0;
for (number i = 0; i < 5; i++) {
  if (i == 1) { continue; }
  if (i == 4) { break; }
  total += i;
}
for (number value in array<number>(2, 3)) { total += value; }
if (total == 10) { post("ten"); } else if (total == 0) { post("zero"); } else { post("other"); }
switch (total) { case 10: post("switch"); default: post("default"); }
match (total) { 0 => post("zero"); _ guard total > 5 => post("guarded"); _ => post("small"); }
number Required(optional<number> value) { guard (value != null) else { return -1; } return value; }
post(Required(null));
post(Required(42));
try { report("controlled failure"); } catch (error) { post(error); }
try { post(1 / 0); } catch { post("caught"); }
post(pcall(1 / 0));
post(pcall(4 / 2));
