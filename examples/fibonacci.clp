<< Recursão: os 10 primeiros números de Fibonacci
func fib(n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

void init(){
    for (let mut i = 0; i < 10; i += 1) {
        post(fib(i));
    }   
}

init();