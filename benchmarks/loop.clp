<< Laço aritmético: soma de 0 a 2.999.999
let mut s = 0;
let mut i = 0;
while (i < 3000000) {
  s += i;
  i += 1;
}
post(s);
