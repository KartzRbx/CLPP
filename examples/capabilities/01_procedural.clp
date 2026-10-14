<< Simple procedural program: integer arithmetic, loops and recovery.
number Factorial(number value) {
  number result = 1;
  for (number i in value) { result *= i + 1; }
  return result;
}
post(Factorial(6));
array<number> values = { 4, 8, 15, 16, 23, 42 };
number sum = 0;
for (number value in values) { sum += value; }
post(sum);
try { post(values[99]); }
catch (error) { post("invalid index recovered"); }
post(9007199254740993 + 2);
