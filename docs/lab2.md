# Lab 2: Separate Virtual Size From Resident Pages

## Goal

Extend `vmstat` so user programs can observe:

```c
uint64 resident_pages;
uint64 lazy_faults;
```

## Why

After Lab 1, `proc_size` is no longer enough.  A process can reserve many pages
while only a few are resident.  This lab makes that difference measurable.

## Work

Walk the current process page table and count mapped user leaf pages.  Increment
a per-process counter when a lazy fault successfully creates a page.

Run:

```sh
make grade STAGE=lab2
```

## Hints

Count mapped pages, not page-table pages.  A valid leaf PTE with user
permissions is resident from the process point of view.

For GDB, use `ni` through `faulttrace`, then inspect the PTE returned by `walk`
for a touched address.
