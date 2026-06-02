#ifndef XV6_VM_H
#define XV6_VM_H

#include "types.h"

struct vmstat {
    uint64 free_bytes;
    uint64 proc_size;
    uint64 page_size;
    uint64 resident_pages;
    uint64 fault_count;
};

#endif
