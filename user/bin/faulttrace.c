#include "fcntl.h"
#include "lab.h"
#include "types.h"
#include "user.h"
#include "vm.h"

#define RESERVE_PAGES 8

static void snapshot(struct vmstat* st) {
    if (vmstat(st) < 0) {
        lab_str("status", "vmstat_failed");
        exit(1);
    }
}

int main(void) {
    struct vmstat before, after_reserve, after_touch, after_read, after_release;
    uint64 reserve_bytes;
    char* arena;
    char* read_page;
    int fd;
    int read_ok;

    snapshot(&before);
    reserve_bytes = before.page_size * RESERVE_PAGES;

    arena = sbrk(reserve_bytes);
    if (arena == SBRK_ERROR) {
        lab_str("status", "sbrk_failed");
        exit(1);
    }
    snapshot(&after_reserve);

    arena[0] = 1;
    arena[before.page_size * 3] = 2;
    snapshot(&after_touch);

    read_page = arena + before.page_size * 5;
    fd = open("README", O_RDONLY);
    if (fd < 0) {
        lab_str("status", "open_failed");
        exit(1);
    }
    read_ok = read(fd, read_page, 1) == 1;
    close(fd);
    snapshot(&after_read);

    if (sbrk(-(int)reserve_bytes) == SBRK_ERROR) {
        lab_str("status", "release_failed");
        exit(1);
    }
    snapshot(&after_release);

    lab_u64("reserve_pages", RESERVE_PAGES);
    lab_u64(
        "size_growth_pages",
        (after_reserve.proc_size - before.proc_size) / before.page_size
    );
    lab_u64(
        "resident_after_reserve",
        after_reserve.resident_pages - before.resident_pages
    );
    lab_u64(
        "resident_after_touch",
        after_touch.resident_pages - before.resident_pages
    );
    lab_u64(
        "resident_after_read",
        after_read.resident_pages - before.resident_pages
    );
    lab_u64("resident_after_release", after_release.resident_pages);
    lab_u64(
        "faults_after_reserve",
        after_reserve.lazy_faults - before.lazy_faults
    );
    lab_u64(
        "faults_after_touch",
        after_touch.lazy_faults - before.lazy_faults
    );
    lab_u64("faults_after_read", after_read.lazy_faults - before.lazy_faults);
    lab_i64("read_lazy_buffer", read_ok);
    lab_str("status", "ok");
    exit(0);
}
