// Shell.

#include "fcntl.h"
#include "types.h"
#include "user.h"

#define MAXARGS 10

int fork1(void);
void panic(char*);
void runcmd(char**) __attribute__((noreturn));

void runcmd(char** argv) {
    if (argv[0] == 0)
        exit(1);
    exec(argv[0], argv);
    fprintf(2, "exec %s failed\n", argv[0]);
    exit(1);
}

int getcmd(char* buf, int nbuf) {
    write(2, "$ ", 2);
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if (buf[0] == 0)
        return -1;
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
