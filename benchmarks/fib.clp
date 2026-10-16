<< Chamadas recursivas: fib(27) = 196418
number fib(number n) {
  if (n < 2) { return n; }
  return fib(n - 1) + fib(n - 2);
}
post(fib(27));
