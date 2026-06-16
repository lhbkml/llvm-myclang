// 测试命名规范 — 混合合法和不合法的函数名

int good_name(int x) {          // ✓ snake_case
    return x * 2;
}

int BadName(int x) {            // ✗ 大写开头
    return x * 2;
}

int badCamelCase(int x) {       // ✗ 驼峰
    return x * 2;
}

int UPPER_CASE(int x) {         // ✗ 全大写
    return x * 2;
}

int _leading_underscore(int x) {// ✗ 下划线开头
    return x * 2;
}

int has123digits(int x) {       // ✓ 含数字但合法
    return x * 2;
}
