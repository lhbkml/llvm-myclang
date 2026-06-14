// 共享头文件 — 测试 #include 计数和头文件符号过滤
int global_counter = 0;                    // 全局变量

int helper_add(int a, int b) {            // 头文件中的函数（应被 isFromMainFile 过滤）
    return a + b;
}

int helper_mul(int a, int b) {
    return a * b;
}
