#include "lab.h"
#include "fcntl.h"
#include "types.h"
#include "user.h"
#include "vm.h"

#define RESERVE_PAGES 16
#define TOUCH_PAGES 2

static uint64 kib(uint64 bytes) {
    return bytes / 1024;
}

static uint64 drop_kib(uint64 before, uint64 after) {
    if (before < after)
        return 0;
    return kib(before - after);
}

static void snapshot(struct vmstat* st) {
    if (vmstat(st) < 0) {
        lab_str("status", "vmstat_failed");
        exit(1);
    }
}

int main(void) {
    struct vmstat before, after_reserve, after_touch, after_release;
    struct vmstat before_read, after_read, after_read_release;
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

    arena[0] = 11;
    arena[before.page_size * (RESERVE_PAGES - 1)] = 22;
    snapshot(&after_touch);

    if (sbrk(-(int)reserve_bytes) == SBRK_ERROR) {
        lab_str("status", "release_failed");
        exit(1);
    }
    snapshot(&after_release);

    lab_u64("page_size", before.page_size);
    lab_u64("reserve_pages", RESERVE_PAGES);
    lab_u64("touch_pages", TOUCH_PAGES);
    lab_u64("size_growth_bytes", after_reserve.proc_size - before.proc_size);
    lab_u64(
        "size_release_bytes", after_reserve.proc_size - after_release.proc_size
    );
    lab_u64(
        "reserve_delta_kib",
        drop_kib(before.free_bytes, after_reserve.free_bytes)
    );
    lab_u64(
        "touch_delta_kib",
        drop_kib(after_reserve.free_bytes, after_touch.free_bytes)
    );
    lab_u64(
        "release_delta_kib",
        drop_kib(after_release.free_bytes, after_touch.free_bytes)
    );

    snapshot(&before_read);
    read_page = sbrk(before.page_size);
    if (read_page == SBRK_ERROR) {
        lab_str("status", "read_sbrk_failed");
        exit(1);
    }
    fd = open("README", O_RDONLY);
    if (fd < 0) {
        lab_str("status", "read_open_failed");
        exit(1);
    }
    read_ok = read(fd, read_page, 1) == 1;
    close(fd);
    snapshot(&after_read);
    if (sbrk(-(int)before.page_size) == SBRK_ERROR) {
        lab_str("status", "read_release_failed");
        exit(1);
    }
    snapshot(&after_read_release);

    lab_i64("read_lazy_buffer", read_ok);
    lab_u64(
        "read_delta_kib", drop_kib(before_read.free_bytes, after_read.free_bytes)
    );
    lab_u64(
        "read_release_delta_kib",
        drop_kib(after_read_release.free_bytes, after_read.free_bytes)
    );
    lab_str("status", "ok");
    exit(0);
}
