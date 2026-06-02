#ifndef XV6_USER_USER_H
#define XV6_USER_USER_H

#include "types.h"

#define SBRK_ERROR ((char*)-1)

struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int fstat(int fd, struct stat*);
int dup(int);
int getpid(void);
int shutdown(void);
int consolemode(int);
uint64 freemem(void);
char* sys_sbrk(int);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void* memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
int atoi(const char*);
int memcmp(const void*, const void*, uint);
void* memcpy(void*, const void*, uint);
char* sbrk(int);

// printf.c
void fprintf(int, const char*, ...) __attribute__((format(printf, 2, 3)));
void printf(const char*, ...) __attribute__((format(printf, 1, 2)));

// umalloc.c
void* malloc(uint);
void free(void*);

#endif
