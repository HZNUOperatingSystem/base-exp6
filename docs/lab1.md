# Lab 1: Reserve Heap Now, Allocate Pages Later

## Goal

Make positive `sbrk` reserve virtual heap without allocating every physical
page immediately.  Allocate a zero-filled physical page only when the process
first touches a valid heap address.

## Why

The old heap path pays for maximum requested size up front.  The useful
question is: can the address space promise be larger than the physical memory
already paid?

## Work

Change the grow path so positive `sbrk` updates `p->sz` but skips eager page
allocation.  In the trap path, recognize faults inside the process heap,
allocate one page, map it, and resume the process.  Make kernel copies into user
buffers handle the same lazy case.

Run:

```sh
make grade STAGE=lab1
```

## Hints

Do not make every invalid address valid.  A fault is acceptable only when the
address is below `p->sz` and above the user stack guard/invalid region expected
by the kernel.

For GDB, stop near the trap handler, use `si` into the fault helper, and inspect
`stval` to see the faulting virtual address.
