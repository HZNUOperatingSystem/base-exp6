// Shell.

#include "fcntl.h"
#include "fs.h"
#include "term.h"
#include "types.h"
#include "utf8.h"
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

int blank_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void redraw(char* buf, int len, int cursor) {
    char out[140];
    int n = 0;
    int tail = utf8_text_width(buf, cursor, len);

    out[n++] = '\r';
    out[n++] = '\033';
    out[n++] = '[';
    out[n++] = '2';
    out[n++] = 'K';
    out[n++] = '$';
    out[n++] = ' ';
    memmove(out + n, buf, len);
    n += len;
    if (tail > 0) {
        out[n++] = '\033';
        out[n++] = '[';
        n = term_append_uint(out, n, tail);
        out[n++] = 'D';
    }
    write(2, out, n);
}

void redraw_from(char* buf, int len, int cursor) {
    int tail = utf8_text_width(buf, cursor, len);

    write(2, buf + cursor, len - cursor);
    term_clear_eol();
    term_move_cursor('D', tail);
}

int insert_bytes(char* buf, int* len, int* cursor, int nbuf, char* s, int n) {
    if (*len + n >= nbuf)
        return 0;
    memmove(buf + *cursor + n, buf + *cursor, *len - *cursor);
    memmove(buf + *cursor, s, n);
    *len += n;
    *cursor += n;
    buf[*len] = 0;
    return n;
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

int file_entry(char* name) {
    return strcmp(name, ".") != 0 && strcmp(name, "console") != 0;
}

int match_prefix(char* name, char* prefix, int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (name[i] != prefix[i])
            return 0;
    }
    return 1;
}

int token_start(char* buf, int cursor) {
    int start = cursor;

    while (start > 0 && !blank_char(buf[start - 1]))
        start--;
    return start;
}

int first_token(char* buf, int start) {
    for (int i = 0; i < start; i++) {
        if (command_char(buf[i]))
            return 0;
    }
    return 1;
}

int complete_token(char* buf, int* len, int* cursor, int nbuf) {
    char match[DIRSIZ + 1], name[DIRSIZ + 1];
    struct dirent de;
    int fd, matches = 0;
    int start, prefix_start, token_len, command;
    char quote = 0;

    if (*cursor < *len && !blank_char(buf[*cursor]) && buf[*cursor] != '\'' &&
        buf[*cursor] != '"')
        return 0;

    start = token_start(buf, *cursor);
    prefix_start = start;
    if (buf[start] == '\'' || buf[start] == '"') {
        quote = buf[start];
        prefix_start = start + 1;
        if (*cursor < *len && buf[*cursor] != quote)
            return 0;
    }
    token_len = *cursor - prefix_start;
    command = first_token(buf, start);

    if (token_len >= DIRSIZ)
        return 0;

    for (int i = prefix_start; i < *cursor; i++) {
        if (buf[i] == '/')
            return 0;
    }

    if ((fd = open(".", O_RDONLY)) < 0)
        return 0;

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
            continue;
        direntname(&de, name);
        if ((command && !command_entry(name)) ||
            (!command && !file_entry(name)))
            continue;
        if (!match_prefix(name, buf + prefix_start, token_len))
            continue;
        strcpy(match, name);
        matches++;
    }
    close(fd);

    if (matches == 1) {
        char* suffix = match + token_len;
        int n = strlen(suffix);

        return insert_bytes(buf, len, cursor, nbuf, suffix, n);
    }
    return 0;
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
                int next = utf8_next(buf, cursor, len);

                term_move_cursor('C', utf8_text_width(buf, cursor, next));
                cursor = next;
                continue;
            }
            if (seq[0] == '[' && seq[1] == 'D') {
                int prev = utf8_prev(buf, cursor);

                term_move_cursor('D', utf8_text_width(buf, prev, cursor));
                cursor = prev;
                continue;
            }
            if (seq[0] == '[' && seq[1] == '3') {
                if (read(0, &seq[2], 1) == 1 && seq[2] == '~' && cursor < len) {
                    delete_range(
                        buf, &len, &cursor, cursor, utf8_next(buf, cursor, len)
                    );
                    redraw_from(buf, len, cursor);
                }
                continue;
            }
            continue;
        }

        if (c == '\t') {
            int old_cursor = cursor;
            int old_len = len;
            int n = complete_token(buf, &len, &cursor, nbuf);

            if (n > 0) {
                if (old_cursor == old_len)
                    write(2, buf + old_cursor, n);
                else
                    redraw(buf, len, cursor);
            }
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
                int start = utf8_prev(buf, cursor);
                int width = utf8_text_width(buf, start, cursor);

                delete_range(buf, &len, &cursor, start, cursor);
                cursor = start;
                term_move_cursor('D', width);
                redraw_from(buf, len, cursor);
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
        int old_cursor = cursor;
        int old_len = len;
        int n = utf8_input_len(c);

        input[0] = c;
        for (int i = 1; i < n; i++) {
            if (read(0, &input[i], 1) != 1) {
                n = i;
                break;
            }
        }
        if (insert_bytes(buf, &len, &cursor, nbuf, input, n) > 0) {
            if (old_cursor == old_len)
                write(2, input, n);
            else
                redraw(buf, len, cursor);
        }
    }
    save_history(buf, len);
    consolemode(0);

    return 0;
}

int parsecmd(char* buf, char** argv) {
    int argc = 0;
    char* src = buf;
    char* dst = buf;

    while (*src) {
        while (blank_char(*src))
            src++;
        if (*src == 0)
            break;
        if (argc >= MAXARGS - 1)
            panic("too many args");
        argv[argc++] = dst;
        while (*src && !blank_char(*src)) {
            if (*src == '\'' || *src == '"') {
                char quote = *src++;

                while (*src && *src != quote)
                    *dst++ = *src++;
                if (*src == quote)
                    src++;
            } else {
                *dst++ = *src++;
            }
        }
        if (blank_char(*src))
            src++;
        *dst++ = 0;
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
        write(2, "\n", 1);
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
