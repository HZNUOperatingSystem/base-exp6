#include "fcntl.h"
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
    struct vmstat before, after_touch, after_reclaim, after_refault;
    int fd;
    char* p;
    char first;
    char second;

    snapshot(&before);
    fd = open("README", O_RDONLY);
    if (fd < 0) {
        lab_str("status", "open_failed");
        exit(1);
    }
    p = filemap(fd, before.page_size);
    close(fd);
    if (p == SBRK_ERROR) {
        lab_str("status", "filemap_failed");
        exit(1);
    }

    first = p[0];
    snapshot(&after_touch);
    if (pagereclaim() < 0) {
        lab_str("status", "reclaim_failed");
        exit(1);
    }
    snapshot(&after_reclaim);
    second = p[0];
    snapshot(&after_refault);

    lab_i64("bytes_match", first == second);
    lab_u64("resident_after_touch", after_touch.resident_pages);
    lab_u64("resident_after_reclaim", after_reclaim.resident_pages);
    lab_u64("resident_after_refault", after_refault.resident_pages);
    lab_u64("faults_after_refault", after_refault.fault_count);
    lab_str("status", "ok");
    exit(0);
}
