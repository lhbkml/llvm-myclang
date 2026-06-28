#ifndef INPUT_H
#define INPUT_H

// 按键方向
#define KEY_UP    'w'
#define KEY_DOWN  's'
#define KEY_LEFT  'a'
#define KEY_RIGHT 'd'
#define KEY_QUIT  'q'

void input_setup(void);
int  input_poll(void);
void input_restore(void);

#endif
