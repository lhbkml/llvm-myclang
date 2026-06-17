int test_empty_blocks(int x) {
    if (x > 0) {         // ✗ 空块
    }
    for (int i = 0; i < 5; i++) {  // ✗ 空块
    }
    while (x < 10) {     // ✗ 空块
    }
    if (x < 0) {         // ✓ 有内容
        x = -x;
    } else {             // ✗ else 空块
    }
    return x;
}
