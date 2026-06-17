#include <stdio.h>

int main() {
    printf("hello\n");
    return 0;   // ← C99+ 冗余，main 隐式返回 0
}

int not_main(int x) {
    return 0;   // 非 main，不冗余
}

int main_like(char **argv) {
    return 0;   // 非 main，不冗余
}
