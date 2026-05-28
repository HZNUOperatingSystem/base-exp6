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
    struct vmstat before, after_map, after_touch;
    char expected;
    char got;
    char* mapped;
    int fd;

    fd = open("README", O_RDONLY);
    if (fd < 0) {
        lab_str("status", "open_failed");
        exit(1);
    }
    if (read(fd, &expected, 1) != 1) {
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
    mapped = filemap(fd, before.page_size);
    close(fd);
    if (mapped == SBRK_ERROR) {
        lab_str("status", "filemap_failed");
        exit(1);
    }
    snapshot(&after_map);

    got = mapped[0];
    snapshot(&after_touch);

    lab_u64("size_growth_pages", (after_map.proc_size - before.proc_size) / before.page_size);
    lab_u64("resident_after_map", after_map.resident_pages - before.resident_pages);
    lab_u64("resident_after_touch", after_touch.resident_pages - before.resident_pages);
    lab_u64("faults_after_map", after_map.lazy_faults - before.lazy_faults);
    lab_u64("faults_after_touch", after_touch.lazy_faults - before.lazy_faults);
    lab_i64("byte_matches", got == expected);
    lab_u64("mapped_byte", (uchar)got);
    lab_str("status", "ok");
    exit(0);
}
