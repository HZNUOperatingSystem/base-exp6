#include "fcntl.h"
#include "lab.h"
#include "types.h"
#include "user.h"
#include "vm.h"

static uint64 kib(uint64 n) { return n / 1024; }

static uint64 drop_kib(uint64 before, uint64 after) {
    if (before <= after)
        return 0;
    return kib(before - after);
}

static void snapshot(struct vmstat* st) {
    if (vmstat(st) < 0) {
        lab_str("status", "vmstat_failed");
        exit(1);
    }
}

static char* map_readme(uint64 len) {
    int fd = open("README", O_RDONLY);
    char* p;

    if (fd < 0)
        return SBRK_ERROR;
    p = filemap(fd, len);
    close(fd);
    return p;
}

int main(void) {
    struct vmstat before, after_first, after_second;
    char* a;
    char* b;
    char ca;
    char cb;

    snapshot(&before);
    a = map_readme(before.page_size);
    b = map_readme(before.page_size);
    if (a == SBRK_ERROR || b == SBRK_ERROR) {
        lab_str("status", "filemap_failed");
        exit(1);
    }

    ca = a[0];
    snapshot(&after_first);
    cb = b[0];
    snapshot(&after_second);

    lab_i64("bytes_match", ca == cb);
    lab_u64("first_cost_kib", drop_kib(before.free_bytes, after_first.free_bytes));
    lab_u64(
        "second_cost_kib", drop_kib(after_first.free_bytes, after_second.free_bytes)
    );
    lab_u64("resident_after_second", after_second.resident_pages);
    lab_str("status", "ok");
    exit(0);
}
