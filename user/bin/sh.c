// Shell.

#include "fcntl.h"
#include "fs.h"
#include "types.h"
#include "user.h"

#define MAXARGS 10
#define HISTSIZE 16
#define PROMPT "$ "
#define C(x) ((x) - '@')

int fork1(void);
void panic(char*);
void runcmd(char**) __attribute__((noreturn));

char history[HISTSIZE][100];
int nhistory;

int command_char(char c) {
    return c != 0 && c != ' ' && c != '\t' && c != '\r' && c != '\n';
}

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

int nextchar(char* buf, int pos, int len) {
    if (pos >= len)
        return len;
    pos++;
    while (pos < len && utf8_cont(buf[pos]))
        pos++;
    return pos;
}

int prevchar(char* buf, int pos) {
    if (pos <= 0)
        return 0;
    pos--;
    while (pos > 0 && utf8_cont(buf[pos]))
        pos--;
    return pos;
}

uint decodechar(char* buf, int pos, int end) {
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

int rune_width(uint r) {
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

int text_width(char* buf, int start, int end) {
    int width = 0;

    while (start < end) {
        int next = nextchar(buf, start, end);
        width += rune_width(decodechar(buf, start, next));
        start = next;
    }
    return width;
}

void redraw(char* buf, int len, int cursor) {
    int tail = text_width(buf, cursor, len);

    write(2, "\r\033[2K" PROMPT, 7);
    write(2, buf, len);
    if (tail > 0)
        fprintf(2, "\033[%dD", tail);
}

void insert_bytes(char* buf, int* len, int* cursor, int nbuf, char* s, int n) {
    if (*len + n >= nbuf)
        return;
    memmove(buf + *cursor + n, buf + *cursor, *len - *cursor);
    memmove(buf + *cursor, s, n);
    *len += n;
    *cursor += n;
    buf[*len] = 0;
}

void delete_range(char* buf, int* len, int* cursor, int start, int end) {
    memmove(buf + start, buf + end, *len - end);
    *len -= end - start;
    if (*cursor > *len)
        *cursor = *len;
    buf[*len] = 0;
}

int blank_line(char* buf, int len) {
    for (int i = 0; i < len; i++) {
        if (buf[i] != ' ' && buf[i] != '\t')
            return 0;
    }
    return 1;
}

void save_history(char* buf, int len) {
    if (len > 0 && buf[len - 1] == '\n')
        len--;
    if (len == 0 || blank_line(buf, len))
        return;
    if (nhistory > 0 && strlen(history[nhistory - 1]) == len &&
        memcmp(history[nhistory - 1], buf, len) == 0)
        return;
    if (nhistory == HISTSIZE) {
        for (int i = 1; i < HISTSIZE; i++)
            strcpy(history[i - 1], history[i]);
        nhistory--;
    }
    memmove(history[nhistory], buf, len);
    history[nhistory][len] = 0;
    nhistory++;
}

void set_line(char* buf, int* len, int* cursor, char* src, int nbuf) {
    *len = strlen(src);
    if (*len >= nbuf)
        *len = nbuf - 1;
    memmove(buf, src, *len);
    buf[*len] = 0;
    *cursor = *len;
}

void direntname(struct dirent* de, char* name) {
    int i;

    for (i = 0; i < DIRSIZ && de->name[i]; i++)
        name[i] = de->name[i];
    name[i] = 0;
}

int command_entry(char* name) {
    return strcmp(name, ".") != 0 && strcmp(name, "console") != 0 &&
           strchr(name, '.') == 0;
}

int match_prefix(char* name, char* prefix, int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (name[i] != prefix[i])
            return 0;
    }
    return 1;
}

void completecmd(char* buf, int* len, int* cursor, int nbuf) {
    char match[DIRSIZ + 1], name[DIRSIZ + 1];
    struct dirent de;
    int fd, matches = 0;

    if (*cursor != *len || *len >= DIRSIZ)
        return;

    for (int i = 0; i < *len; i++) {
        if (!command_char(buf[i]))
            return;
    }

    if ((fd = open(".", O_RDONLY)) < 0)
        return;

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
            continue;
        direntname(&de, name);
        if (!command_entry(name) || !match_prefix(name, buf, *len))
            continue;
        strcpy(match, name);
        matches++;
    }
    close(fd);

    if (matches == 1) {
        char* suffix = match + *len;
        int n = strlen(suffix);

        if (*len + n < nbuf) {
            insert_bytes(buf, len, cursor, nbuf, suffix, n);
        }
    }
}

void runcmd(char** argv) {
    if (argv[0] == 0)
        exit(1);
    exec(argv[0], argv);
    fprintf(2, "exec %s failed\n", argv[0]);
    exit(1);
}

int getcmd(char* buf, int nbuf) {
    int len = 0, cursor = 0;
    int hpos = nhistory, saved = 0;
    char draft[100];
    char c;

    consolemode(1);
    write(2, PROMPT, 2);
    memset(buf, 0, nbuf);
    memset(draft, 0, sizeof(draft));

    while (len + 1 < nbuf) {
        if (read(0, &c, 1) != 1) {
            consolemode(0);
            return -1;
        }

        if (c == '\033') {
            char seq[3];

            if (read(0, &seq[0], 1) != 1 || read(0, &seq[1], 1) != 1)
                continue;
            if (seq[0] == '[' && seq[1] == 'A') {
                if (hpos > 0) {
                    if (!saved) {
                        memmove(draft, buf, len);
                        draft[len] = 0;
                        saved = 1;
                    }
                    hpos--;
                    set_line(buf, &len, &cursor, history[hpos], nbuf);
                    redraw(buf, len, cursor);
                }
                continue;
            }
            if (seq[0] == '[' && seq[1] == 'B') {
                if (hpos < nhistory) {
                    hpos++;
                    if (hpos == nhistory)
                        set_line(buf, &len, &cursor, draft, nbuf);
                    else
                        set_line(buf, &len, &cursor, history[hpos], nbuf);
                    redraw(buf, len, cursor);
                }
                continue;
            }
            if (seq[0] == '[' && seq[1] == 'C') {
                cursor = nextchar(buf, cursor, len);
                redraw(buf, len, cursor);
                continue;
            }
            if (seq[0] == '[' && seq[1] == 'D') {
                cursor = prevchar(buf, cursor);
                redraw(buf, len, cursor);
                continue;
            }
            if (seq[0] == '[' && seq[1] == '3') {
                if (read(0, &seq[2], 1) == 1 && seq[2] == '~' && cursor < len) {
                    delete_range(
                        buf, &len, &cursor, cursor, nextchar(buf, cursor, len)
                    );
                    redraw(buf, len, cursor);
                }
                continue;
            }
            continue;
        }

        if (c == '\t') {
            completecmd(buf, &len, &cursor, nbuf);
            redraw(buf, len, cursor);
            continue;
        }
        if (c == C('D')) {
            consolemode(0);
            return -1;
        }
        if (c == C('U')) {
            len = cursor = 0;
            buf[0] = 0;
            redraw(buf, len, cursor);
            continue;
        }
        if (c == C('H') || c == '\x7f') {
            if (cursor > 0) {
                int start = prevchar(buf, cursor);

                delete_range(buf, &len, &cursor, start, cursor);
                cursor = start;
                redraw(buf, len, cursor);
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            buf[len++] = '\n';
            buf[len] = 0;
            write(2, "\n", 1);
            break;
        }

        char input[4];
        int n = utf8_input_len(c);

        input[0] = c;
        for (int i = 1; i < n; i++) {
            if (read(0, &input[i], 1) != 1) {
                n = i;
                break;
            }
        }
        insert_bytes(buf, &len, &cursor, nbuf, input, n);
        redraw(buf, len, cursor);
    }
    save_history(buf, len);
    consolemode(0);

    return 0;
}

int parsecmd(char* buf, char** argv) {
    int argc = 0;
    char* p = buf;

    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            *p++ = 0;
        if (*p == 0)
            break;
        if (argc >= MAXARGS - 1)
            panic("too many args");
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            p++;
    }
    argv[argc] = 0;
    return argc;
}

int main(void) {
    static char buf[100];
    char* argv[MAXARGS];
    int fd;

    while ((fd = open("console", O_RDWR)) >= 0) {
        if (fd >= 3) {
            close(fd);
            break;
        }
    }

    while (getcmd(buf, sizeof(buf)) >= 0) {
        if (parsecmd(buf, argv) == 0)
            continue;
        if (fork1() == 0)
            runcmd(argv);
        wait(0);
    }
    exit(0);
}

void panic(char* s) {
    fprintf(2, "%s\n", s);
    exit(1);
}

int fork1(void) {
    int pid = fork();
    if (pid == -1)
        panic("fork");
    return pid;
}
