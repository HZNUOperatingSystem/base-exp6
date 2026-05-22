// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and other kernel allocations. Allocates whole 4096-byte pages.

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "types.h"

void freerange(void* pa_start, void* pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
    struct run* next;
};

struct {
    struct spinlock lock;
    struct run* freelist;
} kmem;

static uint64 phys_top;

void kinit(uint64 dtb_addr) {
    initlock(&kmem.lock, "kmem");
    phys_top = fdt_get_memory_size(dtb_addr);
    if (phys_top == 0)
        phys_top = PHYSTOP;
    freerange(end, (void*)phys_top);
}

void freerange(void* pa_start, void* pa_end) {
    char* p;
    p = (char*)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void* pa) {
    struct run* r;

    if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= phys_top)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void* kalloc(void) {
    struct run* r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if (r)
        kmem.freelist = r->next;
    release(&kmem.lock);

    if (r)
        memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
}

uint64 kphys_top(void) {
    return phys_top;
}

uint64 kfreemem(void) {
    struct run* r;
    uint64 pages = 0;

    acquire(&kmem.lock);
    for (r = kmem.freelist; r; r = r->next)
        pages++;
    release(&kmem.lock);

    return pages * PGSIZE;
}
