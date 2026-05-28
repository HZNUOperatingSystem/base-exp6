#include "lab.h"
#include "types.h"
#include "user.h"
#include "vm.h"

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
    struct vmstat before, after_sbrk, after_touch, after_release;
    char* page;

    snapshot(&before);
    page = sbrk(before.page_size);
    if (page == SBRK_ERROR) {
        lab_str("status", "sbrk_failed");
        exit(1);
    }
    snapshot(&after_sbrk);

    page[0] = 7;
    snapshot(&after_touch);

    if (sbrk(-(int)before.page_size) == SBRK_ERROR) {
        lab_str("status", "release_failed");
        exit(1);
    }
    snapshot(&after_release);

    lab_u64("page_size", before.page_size);
    lab_u64("size_before_bytes", before.proc_size);
    lab_u64("size_after_sbrk_bytes", after_sbrk.proc_size);
    lab_u64("size_after_release_bytes", after_release.proc_size);
    lab_u64("free_before_kib", kib(before.free_bytes));
    lab_u64("free_after_sbrk_kib", kib(after_sbrk.free_bytes));
    lab_u64("free_after_touch_kib", kib(after_touch.free_bytes));
    lab_u64("free_after_release_kib", kib(after_release.free_bytes));
    lab_u64("size_growth_bytes", after_sbrk.proc_size - before.proc_size);
    lab_u64(
        "size_release_bytes", after_sbrk.proc_size - after_release.proc_size
    );
    lab_u64("reserve_delta_kib", drop_kib(before.free_bytes, after_sbrk.free_bytes));
    lab_u64(
        "touch_delta_kib", drop_kib(after_sbrk.free_bytes, after_touch.free_bytes)
    );
    lab_u64(
        "release_delta_kib", drop_kib(after_release.free_bytes, after_touch.free_bytes)
    );
    lab_str("status", "ok");
    exit(0);
}
