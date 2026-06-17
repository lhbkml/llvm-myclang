#include <stdio.h>

void good_void_func() {
    printf("no redundant return\n");
    // 无显式 return — OK
}

void bad_void_func(int x) {
    printf("has redundant return\n");
    if (x > 0) return;   // 提前返回 — 不是末尾，OK
    printf("end\n");
    return;               // ← 冗余
}

int good_int_func(int x) {
    return x * 2;         // 非 void 必须 return — OK
}

void empty_void() {
    return;               // 已经是 [空/仅return]，不重复计数
}
