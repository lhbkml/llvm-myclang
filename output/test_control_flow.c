// 覆盖 do-while, switch, break 的综合测试
#include <stdio.h>

void test_do_while(int n) {
    int i = 0;
    do {
        i++;
    } while (i < n);
}

void test_switch(int x) {
    switch (x) {
    case 1:
        printf("one\n");
        break;
    case 2:
        printf("two\n");
        break;
    case 3:
        printf("three\n");
        break;
    default:
        printf("other\n");
        break;
    }
}

void test_nested_switch(int a, int b) {
    switch (a) {
    case 1:
        switch (b) {
        case 10:
            break;
        case 20:
            break;
        default:
            break;
        }
        break;
    case 2:
        break;
    }
}

void test_loop_break(int limit) {
    for (int i = 0; i < limit; i++) {
        if (i > 5)
            break;
    }
}

int main() {
    test_do_while(3);
    test_switch(2);
    test_nested_switch(1, 10);
    test_loop_break(10);
    return 0;
}
