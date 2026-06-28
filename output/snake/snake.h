#ifndef SNAKE_H
#define SNAKE_H

#define MAX_SNAKE_LEN 100

typedef struct {
    int x, y;
} Point;

typedef struct {
    Point body[MAX_SNAKE_LEN];
    int length;
    int dx, dy;
} Snake;

void snake_init(Snake *s, int startX, int startY);
void snake_move(Snake *s);
void snake_grow(Snake *s);
int  snake_check_self_collision(const Snake *s);
int  snake_check_wall_collision(const Snake *s, int w, int h);
int  snake_head_x(const Snake *s);
int  snake_head_y(const Snake *s);

#endif
