// 覆盖 switch / case / break / if-else
#include "mylib.h"

int switch_test(int cmd) {
    int result = 0;                        // 局部变量

    switch (cmd) {                         // switch 语句
    case 1:
        result = helper_add(10, 20);       // 函数调用
        break;                             // break
    case 2:
        result = helper_mul(5, 6);         // 函数调用
        break;
    case 3:
        if (result > 0) {                  // if 语句
            result = result * 2;
        } else {
            result = 99;
        }
        break;
    default:
        result = -1;
        break;
    }

    return result;                         // return
}
