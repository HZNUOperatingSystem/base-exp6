#include "term.h"
#include "user.h"

int term_append_uint(char* out, int pos, int value) {
    char tmp[12];
    int n = 0;

    if (value == 0) {
        out[pos++] = '0';
        return pos;
    }
    while (value > 0) {
        tmp[n++] = '0' + value % 10;
        value /= 10;
    }
    while (n > 0)
        out[pos++] = tmp[--n];
    return pos;
}

void term_move_cursor(char dir, int width) {
    char out[16];
    int n = 0;

    if (width <= 0)
        return;
    out[n++] = '\033';
    out[n++] = '[';
    n = term_append_uint(out, n, width);
    out[n++] = dir;
    write(2, out, n);
}

void term_clear_eol(void) { write(2, "\033[K", 3); }
