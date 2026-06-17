// 测试全局变量 g_ 前缀规范

int g_count = 0;        // ✓ 有 g_ 前缀
int g_buffer_size = 256; // ✓ 有 g_ 前缀
int bad_var = 1;        // ✗ 缺少 g_
int GLOBAL = 2;          // ✗ 大写
int _private = 3;        // ✗ 下划线开头
