#include "types.h"
#include "utf8.h"

int utf8_cont(char c) { return ((uchar)c & 0xC0) == 0x80; }

int utf8_input_len(char c) {
    uchar u = c;

    if (u < 0x80)
        return 1;
    if ((u & 0xE0) == 0xC0)
        return 2;
    if ((u & 0xF0) == 0xE0)
        return 3;
    if ((u & 0xF8) == 0xF0)
        return 4;
    return 1;
}

int utf8_next(char* buf, int pos, int len) {
    if (pos >= len)
        return len;
    pos++;
    while (pos < len && utf8_cont(buf[pos]))
        pos++;
    return pos;
}

int utf8_prev(char* buf, int pos) {
    if (pos <= 0)
        return 0;
    pos--;
    while (pos > 0 && utf8_cont(buf[pos]))
        pos--;
    return pos;
}

uint utf8_decode(char* buf, int pos, int end) {
    uchar c0 = buf[pos];
    int len = end - pos;
    uint r = 0;

    if (len == 1)
        return c0;
    if (len == 2)
        r = c0 & 0x1F;
    else if (len == 3)
        r = c0 & 0x0F;
    else if (len == 4)
        r = c0 & 0x07;
    else
        return c0;

    for (int i = pos + 1; i < end; i++) {
        if (!utf8_cont(buf[i]))
            return c0;
        r = (r << 6) | (buf[i] & 0x3F);
    }
    return r;
}

int utf8_rune_width(uint r) {
    if (r < 0x20)
        return 0;
    if (r < 0x80)
        return 1;
    if ((r >= 0x1100 && r <= 0x115F) || (r >= 0x2329 && r <= 0x232A) ||
        (r >= 0x2E80 && r <= 0xA4CF) || (r >= 0xAC00 && r <= 0xD7A3) ||
        (r >= 0xF900 && r <= 0xFAFF) || (r >= 0xFE10 && r <= 0xFE19) ||
        (r >= 0xFE30 && r <= 0xFE6F) || (r >= 0xFF00 && r <= 0xFF60) ||
        (r >= 0xFFE0 && r <= 0xFFE6) || (r >= 0x20000 && r <= 0x3FFFD))
        return 2;
    return 1;
}

int utf8_text_width(char* buf, int start, int end) {
    int width = 0;

    while (start < end) {
        int next = utf8_next(buf, start, end);
        width += utf8_rune_width(utf8_decode(buf, start, next));
        start = next;
    }
    return width;
}
