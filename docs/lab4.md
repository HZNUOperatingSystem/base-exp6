# Lab 4: Fork Without Copying Everything

## Problem

Once a process has loaded or touched a large address space, `fork` becomes
expensive because xv6 copies every resident user page.  For branching work, the
child often reads most pages and writes only a few.

Can parent and child share pages after `fork` until one side writes?

## Work

Implement copy-on-write fork for writable user pages:

```text
share the physical page
clear PTE_W in both parent and child
mark both PTEs as copy-on-write
increment a physical-page reference count
on write fault, copy only that page if it is shared
restore write permission for the private copy
```

Kernel `copyout` must also handle copy-on-write destinations.

Run:

```sh
cowcheck
make grade STAGE=lab4
```

## Existing Interfaces You May Use

Useful allocator and page-table interfaces for copy-on-write:

- `PHYSTOP`, `KERNBASE`, `PGSIZE`, `end`, `phys_top`: physical-memory bounds for
  allocator bookkeeping.
- `struct run`, `kmem.freelist`: xv6's physical-page freelist structure.
- `struct spinlock`, `acquire`, `release`: protect allocator state and page
  reference counts.
- `kalloc()`, `kfree(pa)`: allocate and release physical pages; these may need
  reference-count behavior.
- `memset`, `memmove`: debug-fill pages and copy one page when COW breaks.
- `walk`, `walkaddr`, `mappages`, `uvmunmap`: inspect, translate, install, and
  remove user mappings.
- `PTE_V`, `PTE_R`, `PTE_W`, `PTE_U`, `PTE_FLAGS`, `PTE2PA`, `PA2PTE`: inspect
  and rewrite PTE permissions.
- PTE flag definitions in `kernel/riscv/riscv.h`: inspect these if you add a
  software bit for copy-on-write policy that hardware ignores.
- `sfence_vma()`: flush stale translations after changing PTE permissions.
- `uvmcopy(old, new, sz)`: fork's address-space copy path.
- `vmfault(pagetable, va, read)`: handle write faults on COW pages.
- `copyout`: kernel writes to user memory must also resolve COW.
- Small helper functions are encouraged, for example physical-page refcount
  helpers and a one-page COW resolver.

## Hints

Use a software PTE bit for copy-on-write metadata.  The hardware will cause the
write fault because `PTE_W` is clear.

Reference counts belong to physical pages, not processes.

For GDB, disassemble around `sepc` on the write fault and inspect the PTE flags
before and after the fault handler.
