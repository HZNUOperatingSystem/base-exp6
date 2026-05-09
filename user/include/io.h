#ifndef XV6_USER_IO_H
#define XV6_USER_IO_H

#include "types.h"

int read_exact(int fd, void* data, uint n);
int read_exact_progress(int fd, void* data, uint n, char* prefix, char* label);
void* xmalloc(uint n);

#endif
