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
    char expected;
    char first;
    char second;
    char* mapped;
    int fd;

    fd = open("README", O_RDONLY);
    if (fd < 0 || read(fd, &expected, 1) != 1) {
        lab_str("status", "read_expected_failed");
        exit(1);
    }
    close(fd);

    fd = open("README", O_RDONLY);
    if (fd < 0) {
        lab_str("status", "open_failed");
        exit(1);
    }
    snapshot(&before);
    mapped = filemap(fd, 4096);
    close(fd);
    if (mapped == SBRK_ERROR) {
        lab_str("status", "filemap_failed");
        exit(1);
    }

    first = mapped[0];
    snapshot(&after_touch);

    if (pagereclaim() < 0) {
        lab_str("status", "reclaim_failed");
        exit(1);
    }
    snapshot(&after_reclaim);

    second = mapped[0];
    snapshot(&after_refault);

    lab_u64("resident_after_touch", after_touch.resident_pages - before.resident_pages);
    lab_u64("resident_after_reclaim", after_reclaim.resident_pages - before.resident_pages);
    lab_u64("resident_after_refault", after_refault.resident_pages - before.resident_pages);
    lab_u64("faults_after_touch", after_touch.lazy_faults - before.lazy_faults);
    lab_u64("faults_after_reclaim", after_reclaim.lazy_faults - before.lazy_faults);
    lab_u64("faults_after_refault", after_refault.lazy_faults - before.lazy_faults);
    lab_i64("first_byte_matches", first == expected);
    lab_i64("second_byte_matches", second == expected);
    lab_str("status", "ok");
    exit(0);
}
