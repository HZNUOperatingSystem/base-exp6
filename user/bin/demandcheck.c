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

int main(void) {
    struct vmstat before, after_reserve, after_touch, before_read, after_read;
    uint64 reserve_pages;
    uint64 reserve_bytes;
    char* arena;
    char* read_page;
    int fd;

    snapshot(&before);
    reserve_pages = 16;
    reserve_bytes = reserve_pages * before.page_size;
    arena = sbrk(reserve_bytes);
    if (arena == SBRK_ERROR) {
        lab_str("status", "sbrk_failed");
        exit(1);
    }
    snapshot(&after_reserve);

    arena[0] = 1;
    arena[8 * before.page_size] = 2;
    snapshot(&after_touch);

    read_page = sbrk(before.page_size);
    if (read_page == SBRK_ERROR) {
        lab_str("status", "read_sbrk_failed");
        exit(1);
    }
    snapshot(&before_read);
    fd = open("README", O_RDONLY);
    if (fd < 0 || read(fd, read_page, 1) != 1) {
        lab_str("status", "read_failed");
        exit(1);
    }
    close(fd);
    snapshot(&after_read);

    lab_u64("reserve_pages", reserve_pages);
    lab_u64("size_growth_kib", kib(after_reserve.proc_size - before.proc_size));
    lab_u64("reserve_cost_kib", drop_kib(before.free_bytes, after_reserve.free_bytes));
    lab_u64("touch_cost_kib", drop_kib(after_reserve.free_bytes, after_touch.free_bytes));
    lab_u64("read_cost_kib", drop_kib(before_read.free_bytes, after_read.free_bytes));
    lab_str("status", "ok");
    exit(0);
}
