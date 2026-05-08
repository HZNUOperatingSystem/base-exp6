#include "types.h"
#include "user.h"

int main(void) {
    uint64 bytes = freemem();

    printf("free: %d KiB (%d bytes)\n", (int)(bytes / 1024), (int)bytes);
    exit(0);
}
