#include "ctype.h"

int char_printable(char c) { return c >= 32 && c <= 126; }

int char_whitespace(char c) {
    return c == '\n' || c == '\r' || c == '\t' || c == ' ';
}

int hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
