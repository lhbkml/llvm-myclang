#include "food.h"
#include <stdlib.h>
#include <time.h>

static int g_seeded = 0;

void food_spawn(int *x, int *y, int w, int h) {
    if (!g_seeded) { srand(time(NULL)); g_seeded = 1; }
    *x = rand() % w;
    *y = rand() % h;
}
