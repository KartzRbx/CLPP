<< Laço aritmético: soma de 0 a 2.999.999
auto s = 0;
auto i = 0;
while (i < 3000000) {
  s += i;
  i += 1;
}
post(s);
