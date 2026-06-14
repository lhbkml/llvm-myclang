// 主入口 — 调用前两个文件的函数 + 局部/全局变量
#include <stdio.h>
#include "mylib.h"

char *app_name = "MultiTest";              // 全局变量

int main() {
    int a = loop_test(10);                 // 局部变量 + 函数调用
    int b = switch_test(2);                // 局部变量 + 函数调用
    int total = a + b;                     // 局部变量

    if (total > 0) {                       // if 语句
        printf("Total: %d\n", total);      // 函数调用
    }

    return 0;                              // return
}
