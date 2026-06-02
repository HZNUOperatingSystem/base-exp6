# Lab 5: Keep Addresses, Drop Unused Pages

## Problem

A long-running runtime may keep a large logical arena with stable addresses.
Some blocks become empty, but shrinking the whole heap would destroy the address
layout.

Can user code tell the kernel that a range should remain valid but its resident
anonymous pages may be forgotten?

## Work

Implement:

```c
int pagediscard(void *addr, uint64 len);
```

For anonymous heap pages in the range:

```text
validate the range
round to page boundaries
unmap resident pages
free their physical memory
leave holes inside p->sz
```

Future reads or writes to discarded heap pages should fault them back as
zero-filled demand pages.

Run:

```sh
discardcheck
make grade STAGE=lab5
```

## Hints

Discard is not `sbrk(-n)`: the virtual size should not shrink.

Ignore holes that are already nonresident.  The operation should be useful on a
sparse range.

For GDB, step through range rounding and validation first; most early bugs are
off-by-one errors near page boundaries.
