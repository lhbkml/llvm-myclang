// 覆盖 for / while / do-while / continue / break / goto
#include "mylib.h"

int loop_test(int limit) {
    int i, sum = 0;                        // 2个局部变量

    for (i = 0; i < limit; i++) {          // for 循环
        if (i == 3)
            continue;                      // continue
        if (i == 7)
            break;                         // break
        sum += i;
    }

    int j = 0;                             // 局部变量
    while (j < 5) {                        // while 循环
        j++;
    }

    int k = 0;                             // 局部变量
    do {                                   // do-while 循环
        k++;
    } while (k < 3);

    if (limit < 0)
        goto cleanup;                      // goto

    return sum;                            // return

cleanup:
    return -1;                             // return
}
