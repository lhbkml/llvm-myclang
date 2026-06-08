#include <stdio.h>
#include <stdlib.h>

int global_counter = 0;

int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}

void print_hello() {
    printf("Hello from analyzed code!\n");
}

int main() {
    int x = 10;
    int y = 20;
    int z;

    if (x > 0) {
        z = x + y;
    } else {
        z = 0;
    }

    for (int i = 0; i < 5; i++) {
        global_counter++;
    }

    int w = x;
    while (w > 0) {
        w--;
    }

    int f = factorial(5);
    print_hello();

    return 0;
}
