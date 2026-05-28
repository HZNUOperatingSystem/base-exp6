#include "lab.h"
#include "types.h"
#include "user.h"
#include "vm.h"

#define ARENA_PAGES 16
#define TOUCH_PAGES 8
#define KEEP_PAGES 2

static void snapshot(struct vmstat* st) {
    if (vmstat(st) < 0) {
        lab_str("status", "vmstat_failed");
        exit(1);
    }
}

int main(void) {
    struct vmstat before, after_touch, after_discard, after_retouch;
    char* arena;
    uint64 arena_bytes;

    snapshot(&before);
    arena_bytes = before.page_size * ARENA_PAGES;
    arena = sbrk(arena_bytes);
    if (arena == SBRK_ERROR) {
        lab_str("status", "sbrk_failed");
        exit(1);
    }

    for (int i = 0; i < TOUCH_PAGES; i++)
        arena[i * before.page_size] = i + 1;
    snapshot(&after_touch);

    if (pagediscard(
            arena + KEEP_PAGES * before.page_size,
            (TOUCH_PAGES - KEEP_PAGES) * before.page_size
        ) < 0) {
        lab_str("status", "discard_failed");
        exit(1);
    }
    snapshot(&after_discard);

    arena[(KEEP_PAGES + 1) * before.page_size] = 99;
    snapshot(&after_retouch);

    lab_u64("arena_pages", ARENA_PAGES);
    lab_u64("touched_pages", TOUCH_PAGES);
    lab_u64("kept_pages", KEEP_PAGES);
    lab_u64("size_growth_pages", (after_touch.proc_size - before.proc_size) / before.page_size);
    lab_u64("resident_after_touch", after_touch.resident_pages - before.resident_pages);
    lab_u64("resident_after_discard", after_discard.resident_pages - before.resident_pages);
    lab_u64("resident_after_retouch", after_retouch.resident_pages - before.resident_pages);
    lab_u64("faults_after_touch", after_touch.lazy_faults - before.lazy_faults);
    lab_u64("faults_after_discard", after_discard.lazy_faults - before.lazy_faults);
    lab_u64("faults_after_retouch", after_retouch.lazy_faults - before.lazy_faults);
    lab_str("status", "ok");
    exit(0);
}
