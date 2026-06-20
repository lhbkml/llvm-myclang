// Test: dead return detection via CFG reachability

// Case 1: return after if-else with returns in both branches
int after_if_else(int x) {
    if (x > 0)
        return 1;
    else
        return 2;
    return 3;  // DEAD
}

// Case 2: return after unconditional return
void after_return(void) {
    return;
    return;  // DEAD
}

// Case 3: normal reachable return — should NOT be flagged
int normal(int x) {
    if (x > 0)
        return 1;
    return 0;  // reachable
}

// Case 4: return in all switch cases makes subsequent return dead
int after_switch(int x) {
    switch (x) {
        case 1: return 10;
        case 2: return 20;
        default: return 30;
    }
    return 40;  // DEAD
}

// Case 5: while(1) with break + return after
int while_one(int x) {
    while (1) {
        if (x-- <= 0)
            break;
    }
    return 0; // reachable (break exits the loop)
}

// Case 6: dead assignments and function calls, not just returns
int dead_assignments(int x) {
    if (x > 0)
        return 1;
    else
        return 2;
    x = 42;        // DEAD: assignment
    x++;           // DEAD: increment
    return x;      // DEAD: return
}

int main() {
    return 0;
}
