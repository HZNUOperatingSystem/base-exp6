# Lab 5: Keep Addresses, Drop Unused Pages

## Problem

After demand allocation, file-backed mappings, shared clean pages, and
copy-on-write fork, the system can avoid many unnecessary copies.  One problem
is still visible in long-running runtimes: they often keep a large logical arena
with stable addresses.  Some blocks become empty, but shrinking the whole heap
would destroy the address layout.

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

This is the last lab in the sequence.  At this point the model workload should
be explainable as a set of virtual-memory choices: reservation, first touch,
file-backed demand loading, sharing, write-time copying, and explicit discard.

For final submission, run:

```sh
make submit
```

This always runs a fresh full grade first, writes `results.json` in the project
root, and then creates `xv6-vm-submit.tar.gz`.  Full grading uses a 10 second
timeout for each QEMU probe; a timed-out stage receives zero credit for that
stage.
