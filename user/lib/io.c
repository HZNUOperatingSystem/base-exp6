#include "io.h"
#include "user.h"

int read_exact(int fd, void* data, uint n) {
    char* p = data;
    uint off = 0;

    while (off < n) {
        int chunk = n - off;
        int cc;

        if (chunk > 4096)
            chunk = 4096;
        cc = read(fd, p + off, chunk);
        if (cc <= 0)
            return -1;
        off += cc;
    }
    return 0;
}

void* xmalloc(uint n) {
    void* p = malloc(n);

    if (p == 0) {
        fprintf(2, "malloc failed\n");
        exit(1);
    }
    memset(p, 0, n);
    return p;
}
