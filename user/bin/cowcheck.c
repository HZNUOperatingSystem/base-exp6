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
    struct vmstat before, after_touch, after_wait;
    char* arena;
    int status = -1;
    int pid;

    snapshot(&before);
    arena = sbrk(8 * before.page_size);
    if (arena == SBRK_ERROR) {
        lab_str("status", "sbrk_failed");
        exit(1);
    }
    for (int i = 0; i < 8; i++)
        arena[i * before.page_size] = i + 1;
    snapshot(&after_touch);

    pid = fork();
    if (pid < 0) {
        lab_str("status", "fork_failed");
        exit(1);
    }
    if (pid == 0) {
        struct vmstat child_before, child_after;
        snapshot(&child_before);
        arena[3 * before.page_size] = 99;
        snapshot(&child_after);
        lab_u64(
            "child_write_cost_kib",
            drop_kib(child_before.free_bytes, child_after.free_bytes)
        );
        exit(arena[0] == 1 && arena[3 * before.page_size] == 99 ? 0 : 1);
    }

    wait(&status);
    snapshot(&after_wait);
    lab_u64("touch_cost_kib", drop_kib(before.free_bytes, after_touch.free_bytes));
    lab_i64("child_status", status);
    lab_i64("parent_value_preserved", arena[3 * before.page_size] == 4);
    lab_u64("parent_resident_pages", after_wait.resident_pages);
    lab_str("status", "ok");
    exit(0);
}
