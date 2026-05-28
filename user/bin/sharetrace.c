#include "fcntl.h"
#include "lab.h"
#include "types.h"
#include "user.h"
#include "vm.h"

static uint64 drop_kib(uint64 before, uint64 after) {
    if (before < after)
        return 0;
    return (before - after) / 1024;
}

static void snapshot(struct vmstat* st) {
    if (vmstat(st) < 0) {
        lab_str("status", "vmstat_failed");
        exit(1);
    }
}

static char* map_readme_page(void) {
    int fd = open("README", O_RDONLY);
    char* p;

    if (fd < 0) {
        lab_str("status", "open_failed");
        exit(1);
    }
    p = filemap(fd, 4096);
    close(fd);
    if (p == SBRK_ERROR) {
        lab_str("status", "filemap_failed");
        exit(1);
    }
    return p;
}

int main(void) {
    struct vmstat before, after_map, after_first, after_second;
    char expected;
    char* a;
    char* b;
    int fd;
    int ok;

    fd = open("README", O_RDONLY);
    if (fd < 0 || read(fd, &expected, 1) != 1) {
        lab_str("status", "read_expected_failed");
        exit(1);
    }
    close(fd);

    snapshot(&before);
    a = map_readme_page();
    b = map_readme_page();
    snapshot(&after_map);

    ok = a[0] == expected;
    snapshot(&after_first);
    ok = ok && b[0] == expected;
    snapshot(&after_second);

    lab_u64("mapped_pages", (after_map.proc_size - before.proc_size) / before.page_size);
    lab_u64("resident_after_map", after_map.resident_pages - before.resident_pages);
    lab_u64("resident_after_first", after_first.resident_pages - before.resident_pages);
    lab_u64("resident_after_second", after_second.resident_pages - before.resident_pages);
    lab_u64("faults_after_second", after_second.lazy_faults - before.lazy_faults);
    lab_u64("physical_cost_kib", drop_kib(before.free_bytes, after_second.free_bytes));
    lab_i64("bytes_match", ok);
    lab_str("status", "ok");
    exit(0);
}
