#ifndef XV6_USER_UTF8_H
#define XV6_USER_UTF8_H

#include "types.h"

int utf8_cont(char c);
int utf8_input_len(char c);
int utf8_next(char* buf, int pos, int len);
int utf8_prev(char* buf, int pos);
uint utf8_decode(char* buf, int pos, int end);
int utf8_rune_width(uint r);
int utf8_text_width(char* buf, int start, int end);
void utf8_safe_print(char* s);

#endif
