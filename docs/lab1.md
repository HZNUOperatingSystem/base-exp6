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

Run:

```sh
demandcheck
next model.bin tokenizer.bin 1 "hello"
make grade STAGE=lab1
```

## Hints

Do not make every bad address valid.  A fault is acceptable only if the address
belongs to the process's promised heap.

Negative `sbrk` and process exit must tolerate holes.  A lazy heap has virtual
pages with no PTE yet.

For GDB, stop in `usertrap`, inspect `scause` and `stval`, then `si` into
`vmfault`.
