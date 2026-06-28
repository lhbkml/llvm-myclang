#include "board.h"
#include "snake.h"
#include <stdio.h>
#include <stdlib.h>

void board_init(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

void board_draw(const Snake *s, int foodX, int foodY, int score) {
    board_init();
    // 上边框
    printf("  +"); for (int i = 0; i < BOARD_W; i++) printf("-"); printf("+\n");
    // 中间
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        printf("  |");
        for (int x = 0; x < BOARD_W; x++) {
            int isSnake = 0, isHead = 0;
            for (int i = 0; i < s->length; i++) {
                if (s->body[i].x == x && s->body[i].y == y) {
                    isSnake = 1;
                    if (i == 0) isHead = 1;
                    break;
                }
            }
            if (isHead)      printf("O");
            else if (isSnake) printf("o");
            else if (x == foodX && y == foodY) printf("*");
            else              printf(" ");
        }
        printf("|\n");
    }
    // 下边框
    printf("  +"); for (int i = 0; i < BOARD_W; i++) printf("-"); printf("+\n");
    printf("  得分: %d  用 WASD 控制方向, Q 退出\n", score);
}

void board_clear(void) {
    printf("\033[0m\n");
}
