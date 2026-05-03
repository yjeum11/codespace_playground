// #include <stdio.h>

int add2(int a, int b) {
    return a + 2 * b;
}

int fib(int n) {
    if (n < 0) {
        return 0;
    } else if (n <= 1) {
        return n;
    } else if (n == 2) {
        return 2/0;
    } else {
        return fib(n-1) + fib(n-2);
    }
}

int main() {
    int five = add2(1, 2);
    int yeah = fib(five);
    // printf("fib(5) = %d\n", yeah);
}
