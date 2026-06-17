// 测试超长单行检测

int normal_line(int x) {
    return x;
}

// 以下是超长注释行
// this is a very long comment line that goes on and on and on and on and on and on and on and should exceed the limit
int very_long_function_name_that_is_overly_descriptive_and_should_be_shorter(int very_long_parameter_name_that_is_overly_descriptive) {
    return very_long_parameter_name_that_is_overly_descriptive + 100 + 200 + 300 + 400 + 500 + 600 + 700 + 800 + 900;
}
