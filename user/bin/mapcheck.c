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
    int fd;
    char* p;
    char first;

    snapshot(&before);
    fd = open("README", O_RDONLY);
    if (fd < 0) {
        lab_str("status", "open_failed");
        exit(1);
    }
    if (read(fd, &first, 1) != 1) {
        lab_str("status", "read_failed");
        exit(1);
    }
    close(fd);

    fd = open("README", O_RDONLY);
    p = filemap(fd, before.page_size);
    close(fd);
    if (p == SBRK_ERROR) {
        lab_str("status", "filemap_failed");
        exit(1);
    }
    snapshot(&after_map);

    lab_i64("byte_matches", p[0] == first);
    snapshot(&after_touch);
    lab_u64("resident_after_map", after_map.resident_pages);
    lab_u64("resident_after_touch", after_touch.resident_pages);
    lab_u64("faults_after_touch", after_touch.fault_count);
    lab_str("status", "ok");
    exit(0);
}
