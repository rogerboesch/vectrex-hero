//
// terminal.c — Raw mode, input, ANSI helpers
//

#include "game.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <signal.h>

static struct termios orig_termios;
static int raw_mode_active = 0;

static void restore_terminal(void) {
    if (raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        // Show cursor, leave alternate screen
        write(STDOUT_FILENO, "\033[?25h\033[?1049l", 14);
        raw_mode_active = 0;
    }
}

static void sighandler(int sig) {
    (void)sig;
    restore_terminal();
    _exit(1);
}

void term_init(void) {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active = 1;

    // Enter alternate screen, hide cursor, clear
    write(STDOUT_FILENO, "\033[?1049h\033[?25l\033[2J", 20);

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    atexit(restore_terminal);
}

void term_cleanup(void) {
    restore_terminal();
}

int term_read_key(void) {
    char buf[8];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return KEY_NONE;

    if (n == 1) {
        switch (buf[0]) {
            case 'q': case 'Q': return KEY_Q;
            case 'r': case 'R': return KEY_R;
            case ' ':           return KEY_SPACE;
            case 'd': case 'D': return KEY_D;
            case '\r': case '\n': return KEY_ENTER;
            case 27:            return KEY_NONE; // bare ESC
        }
    }
    // Arrow keys: ESC [ A/B/C/D
    if (n >= 3 && buf[0] == 27 && buf[1] == '[') {
        switch (buf[2]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
        }
    }
    return KEY_NONE;
}

void term_get_size(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    } else {
        *cols = 80;
        *rows = 24;
    }
}
