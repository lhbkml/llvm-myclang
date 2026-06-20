// Test: switch fall-through detection

// Case 1: clear fall-through (no break)
int missing_break(int x) {
    int r = 0;
    switch (x) {
        case 1:
            r = 10;   // FALL-THROUGH to case 2
        case 2:
            r = 20;
            break;
        default:
            r = -1;
            break;
    }
    return r;
}

// Case 2: no fall-through (all cases have break/return)
int all_breaks(int x) {
    switch (x) {
        case 1: return 10;
        case 2: return 20;
        default: return -1;
    }
}

// Case 3: multiple fall-throughs
int multi_fallthrough(int x) {
    int r = 0;
    switch (x) {
        case 1:
            r = 1;    // fall-through
        case 2:
            r = 2;    // fall-through
        case 3:
            r = 3;
            break;
        default:
            break;
    }
    return r;
}

int main() {
    return 0;
}
