async func add(a, b) {
  return a + b;
}

task job = spawn add(1, 2);
post(await job);
