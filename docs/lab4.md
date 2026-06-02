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

## Hints

Use a software PTE bit for copy-on-write metadata.  The hardware will cause the
write fault because `PTE_W` is clear.

Reference counts belong to physical pages, not processes.

For GDB, disassemble around `sepc` on the write fault and inspect the PTE flags
before and after the fault handler.
