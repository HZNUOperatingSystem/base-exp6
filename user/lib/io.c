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

int read_exact_progress(int fd, void* data, uint n, char* prefix, char* label) {
    char* p = data;
    uint off = 0;
    uint last_log = 0;

    while (off < n) {
        int chunk = n - off;
        int cc;

        if (chunk > 4096)
            chunk = 4096;
        cc = read(fd, p + off, chunk);
        if (cc <= 0)
            return -1;
        off += cc;
        if (off == n || off - last_log >= 4 * 1024 * 1024) {
            fprintf(
                2,
                "%s: read %s %d/%d KiB\n",
                prefix,
                label,
                (int)(off / 1024),
                (int)(n / 1024)
            );
            last_log = off;
        }
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
