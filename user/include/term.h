#ifndef XV6_USER_TERM_H
#define XV6_USER_TERM_H

int term_append_uint(char* out, int pos, int value);
void term_move_cursor(char dir, int width);
void term_clear_eol(void);

#endif
