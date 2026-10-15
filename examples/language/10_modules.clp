link "./lib/counter.clp" as Counter;
link "./lib/counter.clp" as Again;
link @clpp.axiom as Axiom;
Counter.Item item = Counter.Item { Value = Counter.Next() };
post(item.Value);
post(Again.Next());
post(Counter.LIMIT);
post(Axiom.Clamp(150, 0, 100));
