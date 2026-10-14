async number add(number a, number b) {
  return a + b;
}


post(parallel(add(1, 2), add(10, 20)));
