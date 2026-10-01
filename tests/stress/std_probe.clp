link @clpp.axiom as Axiom;
link @clpp.text as Text;
post("a\n");
post(Text.trim("  hi  "));
post(Text.contains("abcd", "bc"));
post(Axiom.Floor(3.8));
post(Axiom.IsNaN(1));
let values = args();
try { post(values[0]); } catch (e) { post(e); }

