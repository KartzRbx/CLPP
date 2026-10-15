array<number> values = { 3, 1, 2 };
push(values, 4);
insert(values, 0, 9);
post(pop(values));
post(remove(values, 0));
post(find(values, 2));
post(sort(values));
post(values[0 .. 2]);
post(3 .. 6);
dictionary<string, number> scores = dictionary<string, number>("Ada", 7);
scores["Lin"] = 9;
post(scores["Ada"]);
post(remove(scores, "Lin"));
try { post(scores["missing"]); } catch { post("missing key"); }
post("ação"[1]);
post(len("ação"));
post(Vector2(1, 2).y);
post(Vector3(1, 2, 3).z);
post(Vector4(1, 2, 3, 4).w);
