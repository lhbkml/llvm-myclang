#include "snake.h"

void snake_init(Snake *s, int startX, int startY) {
    s->length = 3;
    s->dx = 1; s->dy = 0;
    for (int i = 0; i < s->length; i++) {
        s->body[i].x = startX - i;
        s->body[i].y = startY;
    }
}

void snake_move(Snake *s) {
    for (int i = s->length - 1; i > 0; i--)
        s->body[i] = s->body[i - 1];
    s->body[0].x += s->dx;
    s->body[0].y += s->dy;
}

void snake_grow(Snake *s) {
    if (s->length < MAX_SNAKE_LEN)
        s->length++;
}

int snake_check_self_collision(const Snake *s) {
    for (int i = 1; i < s->length; i++)
        if (s->body[i].x == s->body[0].x && s->body[i].y == s->body[0].y)
            return 1;
    return 0;
}

int snake_check_wall_collision(const Snake *s, int w, int h) {
    int x = s->body[0].x, y = s->body[0].y;
    return (x < 0 || x >= w || y < 0 || y >= h);
}

int snake_head_x(const Snake *s) { return s->body[0].x; }
int snake_head_y(const Snake *s) { return s->body[0].y; }
