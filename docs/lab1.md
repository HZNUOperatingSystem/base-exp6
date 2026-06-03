# Lab 1: Reserve Heap Now, Pay On First Touch

## Problem

Lab 0 explains the `next` failure: the runtime asks for large heap regions
before it has used most of them.  The old `sbrk` path treats a virtual heap
promise as an immediate physical allocation.

Can the kernel promise heap addresses now and allocate physical pages only when
the process actually touches them?

## Work

Make positive `sbrk` grow `p->sz` without calling eager page allocation.

Then handle valid user page faults inside the heap:

```text
faulting address is below p->sz
faulting address is in user heap space
page is not already mapped
allocate one zero-filled page
map it writable and user-accessible
resume the faulting instruction
```

Also make kernel copies into user buffers handle the same lazy heap case.  A
`read(fd, lazy_buffer, 1)` should allocate the destination page instead of
failing.

The runtime's KV cache allocation is intentionally a reservation: the cache is
filled token by token during `llm_forward`, so the loader should not pre-zero
the whole KV arena before inference starts.  If a helper touches every page
during allocation, it has turned the experiment back into eager allocation.

Run:

```sh
demandcheck
next model.bin tokenizer.bin 1 "hello"
make grade STAGE=lab1
```

## Existing Interfaces You May Use

Useful VM and process interfaces already present in this tree:

- `growproc(n)`: changes the current process size for `sbrk`.
- `myproc()`: gets the current process and its `p->sz`/`p->pagetable`.
- `TRAPFRAME`, `MAXVA`, `PGSIZE`: user address-space bounds and page size.
- `PGROUNDDOWN(va)`, `PGROUNDUP(va)`: align addresses to page boundaries.
- `uvmdealloc(pagetable, oldsz, newsz)`: releases pages when the heap shrinks.
- `uvmunmap(pagetable, va, npages, do_free)`: removes mappings in a page range.
- `walk(pagetable, va, alloc)`: finds a PTE for a virtual address.
- `walkaddr(pagetable, va)`: translates a mapped user page to a physical
  address.
- `mappages(pagetable, va, size, pa, perm)`: installs PTEs.
- `kalloc()`, `kfree(pa)`: allocate and free one physical page.
- `memset`, `memmove`: initialize and copy memory in the kernel.
- `PTE_V`, `PTE_R`, `PTE_W`, `PTE_U`, `PTE2PA`: inspect and build page mappings.
- `vmfault(pagetable, va, read)`: the central helper to materialize valid faults.
- `copyout`, `copyin`, `copyinstr`: kernel/user copy paths used by syscalls.
- `r_scause()`, `r_stval()`, `usertrap()`: useful places to observe page faults.

## Hints

Do not make every bad address valid.  A fault is acceptable only if the address
belongs to the process's promised heap.

Negative `sbrk` and process exit must tolerate holes.  A lazy heap has virtual
pages with no PTE yet.

For GDB, stop in `usertrap`, inspect `scause` and `stval`, then `si` into
`vmfault`.
