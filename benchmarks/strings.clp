<< Concatenação e templates: 100.000 strings
let mut n = 0;
for (let mut i = 0; i < 100000; i += 1) {
  let s = `item ${i}` .: "!";
  n += 1;
}
post(n);
