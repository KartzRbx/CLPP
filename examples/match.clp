let score = 2;
match (score) {
  1 ~> post(10);
  2 guard score > 10 ~> post(1);
  2 guard score > 0 ~> post(20);
  _ ~> post(0);
}
