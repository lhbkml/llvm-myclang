#ifndef BOARD_H
#define BOARD_H

#include "snake.h"

#define BOARD_W 40
#define BOARD_HEIGHT 20

void board_init(void);
void board_draw(const Snake *s, int foodX, int foodY, int score);
void board_clear(void);

#endif
