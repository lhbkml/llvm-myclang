int global_count = 0;
double global_ratio = 0.5;
char *global_label = "default";

int max(int a, int b) {
    if (a > b)
        return a;
    else
        return b;
}

int min(int a, int b) {
    if (a < b)
        return a;
    return b;
}

void print_loop(int n) {
    int i;
    for (i = 0; i < n; i++) {
        // do something
    }
}

int sum_range(int start, int end) {
    int total = 0;
    int i = start;
    while (i <= end) {
        total = total + i;
        i = i + 1;
    }
    return total;
}

void check_numbers(int x) {
    if (x > 0) {
        if (x > 100) {
            x = 100;
        }
    } else if (x < 0) {
        x = -x;
    } else {
        x = 0;
    }
}

int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}

int main() {
    int a = max(10, 20);
    int b = min(10, 20);
    int s = sum_range(a, b);
    print_loop(3);
    check_numbers(a);
    check_numbers(-5);
    check_numbers(0);
    int f = factorial(5);
    return a + b + s + f;
}
