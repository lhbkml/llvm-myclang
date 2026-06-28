#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

static struct termios old_tio;

void input_setup(void) {
    tcgetattr(STDIN_FILENO, &old_tio);
    struct termios new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

int input_poll(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) < 1) return -1;
    return c;
}

void input_restore(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}
