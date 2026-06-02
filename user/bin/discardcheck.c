#include "lab.h"
#include "types.h"
#include "user.h"
#include "vm.h"

static void snapshot(struct vmstat* st) {
    if (vmstat(st) < 0) {
        lab_str("status", "vmstat_failed");
        exit(1);
    }
}

int main(void) {
    struct vmstat before, after_touch, after_discard, after_retouch;
    char* arena;
    uint64 pages;

    snapshot(&before);
    pages = 16;
    arena = sbrk(pages * before.page_size);
    if (arena == SBRK_ERROR) {
        lab_str("status", "sbrk_failed");
        exit(1);
    }

    for (int i = 0; i < 8; i++)
        arena[i * before.page_size] = i + 1;
    snapshot(&after_touch);

    if (pagediscard(arena + 2 * before.page_size, 6 * before.page_size) < 0) {
        lab_str("status", "discard_failed");
        exit(1);
    }
    snapshot(&after_discard);

    arena[4 * before.page_size] = 77;
    snapshot(&after_retouch);

    lab_u64("resident_after_touch", after_touch.resident_pages - before.resident_pages);
    lab_u64(
        "resident_after_discard", after_discard.resident_pages - before.resident_pages
    );
    lab_u64(
        "resident_after_retouch", after_retouch.resident_pages - before.resident_pages
    );
    lab_u64("faults_after_retouch", after_retouch.fault_count - before.fault_count);
    lab_i64("virtual_size_kept", after_discard.proc_size == after_touch.proc_size);
    lab_str("status", "ok");
    exit(0);
}
