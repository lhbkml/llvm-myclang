#include "snake.h"
#include "board.h"
#include "input.h"
#include "food.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    Snake snake;
    snake_init(&snake, BOARD_W / 2, BOARD_HEIGHT / 2);

    int foodX, foodY, score = 0;
    food_spawn(&foodX, &foodY, BOARD_W, BOARD_HEIGHT);

    input_setup();
    int running = 1;

    while (running) {
        board_draw(&snake, foodX, foodY, score);

        // 非阻塞输入
        for (int i = 0; i < 10; i++) {
            int key = input_poll();
            if (key == KEY_QUIT) { running = 0; break; }
            if      (key == KEY_UP    && snake.dy == 0) { snake.dx = 0; snake.dy = -1; }
            else if (key == KEY_DOWN  && snake.dy == 0) { snake.dx = 0; snake.dy =  1; }
            else if (key == KEY_LEFT  && snake.dx == 0) { snake.dx = -1; snake.dy = 0; }
            else if (key == KEY_RIGHT && snake.dx == 0) { snake.dx =  1; snake.dy = 0; }
            usleep(8000);  // 80ms 游戏速度
        }

        snake_move(&snake);

        // 吃到食物
        if (snake_head_x(&snake) == foodX && snake_head_y(&snake) == foodY) {
            snake_grow(&snake);
            score++;
            food_spawn(&foodX, &foodY, BOARD_W, BOARD_HEIGHT);
        }

        // 碰撞检测
        if (snake_check_wall_collision(&snake, BOARD_W, BOARD_HEIGHT) ||
            snake_check_self_collision(&snake)) {
            running = 0;
        }
    }

    input_restore();
    board_clear();
    printf("游戏结束! 最终得分: %d\n", score);
    return 0;
}
