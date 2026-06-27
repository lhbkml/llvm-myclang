// Test: parameter count + nesting depth (v5.4)
// MAX_PARAMS=5, MAX_NESTING=4

// ✓ 合规：4 参数，在限制内
int ok_params(int a, int b, int c, int d) {
    return a + b + c + d;
}

// ✗ 超标：7 参数，超标 2 个
int too_many(int a, int b, int c, int d, int e, int f, int g) {
    return a + b + c + d + e + f + g;
}

// ✓ 刚好 5 个，边界值
int edge_params(int a, int b, int c, int d, int e) {
    return a + b + c + d + e;
}

// ✓ 嵌套深度 1，合规
int shallow(void) {
    if (1)
        return 1;
    return 0;
}

// ✗ 嵌套深度 6，超标 2 层
int deep(void) {
    if (1) {               // depth=1
        if (2) {           // depth=2
            if (3) {       // depth=3
                if (4) {   // depth=4
                    if (5) { // depth=5
                        if (6) { // depth=6
                            return 6;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

// ✓ 刚好 4 层，边界值（if + for + while + switch）
int edge_nesting(int x) {
    if (x) {                // depth=1
        for (int i = 0; i < 1; i++) {  // depth=2
            while (0) {    // depth=3
                switch (x) { // depth=4
                    case 0: break;
                }
            }
        }
    }
    return 0;
}

// ✓ 深度为 0
int main(void) {
    return 0;
}
