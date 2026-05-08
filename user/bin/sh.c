// Shell.

#include "fcntl.h"
#include "fs.h"
#include "types.h"
#include "user.h"

#define MAXARGS 10
#define C(x) ((x) - '@')

int fork1(void);
void panic(char*);
void runcmd(char**) __attribute__((noreturn));

int command_char(char c) {
    return c != 0 && c != ' ' && c != '\t' && c != '\r' && c != '\n';
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

void completecmd(char* buf, int* len, int nbuf) {
    char match[DIRSIZ + 1], name[DIRSIZ + 1];
    struct dirent de;
    int fd, matches = 0;

    if (*len >= DIRSIZ)
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
            strcpy(buf + *len, suffix);
            *len += n;
            write(2, suffix, n);
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
    int i = 0;
    char c;

    consolemode(1);
    write(2, "$ ", 2);
    memset(buf, 0, nbuf);

    while (i + 1 < nbuf) {
        if (read(0, &c, 1) != 1) {
            consolemode(0);
            return -1;
        }
        if (c == '\t') {
            completecmd(buf, &i, nbuf);
            continue;
        }
        if (c == C('D')) {
            consolemode(0);
            return -1;
        }
        if (c == C('U')) {
            while (i > 0) {
                i--;
                write(2, "\b \b", 3);
            }
            buf[i] = 0;
            continue;
        }
        if (c == C('H') || c == '\x7f') {
            if (i > 0) {
                i--;
                buf[i] = 0;
                write(2, "\b \b", 3);
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            buf[i++] = '\n';
            write(2, "\n", 1);
            break;
        }
        buf[i++] = c;
        write(2, &c, 1);
    }
    buf[i] = 0;
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
