async number add(number a, number b) {
  return a + b;
}

task<number> job = spawn add(1, 2);
post(await job);
