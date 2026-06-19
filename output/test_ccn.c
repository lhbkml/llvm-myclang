// Test file for cyclomatic complexity
// Expected CCN breakdown:
//   simple_func: 1 (base only, no decisions)
//   if_func:     4 (3 ifs)
//   loop_func:   5 (for + while + do-while + if)
//   logical_func: 4 (if + && + ||)
//   switch_func: 6 (switch with 3 cases + 2 ifs)

int simple_func(int x) {
    return x + 1;
}

int if_func(int x) {
    if (x > 10)
        return 1;
    else if (x > 5)
        return 2;
    else if (x > 0)
        return 3;
    return 0;
}

int loop_func(int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += i;
    }
    while (sum > 100) {
        sum--;
    }
    do {
        sum++;
    } while (sum < 0);
    if (sum == 42)
        return 1;
    return 0;
}

int logical_func(int a, int b, int c) {
    if (a > 0 && b > 0)
        return 1;
    if (a < 0 || c < 0)
        return 2;
    return 0;
}

int switch_func(int x) {
    int result = 0;
    switch (x) {
        case 1:
            result = 10;
            break;
        case 2:
            result = 20;
            break;
        case 3:
        case 4:
            result = 34;
            break;
        default:
            result = -1;
            break;
    }
    if (result > 0)
        result++;
    if (result < 0)
        result--;
    return result;
}

int ternary_func(int x) {
    return (x > 0) ? 1 : (x < 0) ? -1 : 0;
}

// high_ccn_func: should have CCN around 12-13 (超过10的阈值)
int high_ccn_func(int a, int b, int c, int d) {
    if (a > 0) {
        if (b > 0) {
            if (c > 0) {
                return 1;
            } else if (c == 0) {
                return 2;
            }
        } else if (b == 0) {
            if (c > 0 && d > 0)
                return 3;
        }
    }
    if (a < 0 || b < 0) {
        if (c < 0 || d < 0)
            return 4;
        else
            return 5;
    }
    switch (a) {
        case 1: return 6;
        case 2: return 7;
        case 3:
        case 4: return 8;
        default:
            if (b > 0) return 9;
            else return 10;
    }
}

int main() {
    return 0;
}
