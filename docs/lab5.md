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

## Existing Interfaces You May Use

Useful syscall and VM interfaces for page discard:

- `pagediscard(addr, len)`: the user-facing syscall to implement.
- `sys_pagediscard(void)`: the kernel syscall body.
- `myproc()`, `p->sz`, `p->pagetable`: current process bounds and page table.
- `argaddr(n, &x)`: read syscall address/length arguments.
- `PGROUNDDOWN`, `PGROUNDUP`, `PGSIZE`: convert byte ranges to page ranges.
- `walk(pagetable, va, 0)`: inspect existing PTEs without allocating page-table
  pages.
- `PTE_V`, `PTE_U`, `PTE_FILESHARED`, `PTE_COW`: distinguish resident user
  pages, clean shared file pages, and anonymous/COW pages.
- `PTE2PA`, `kfree(pa)`: release anonymous physical pages through the allocator.
- `free_user_page(pte)`: if you built a shared release helper earlier, reuse it
  instead of duplicating release policy.
- `sfence_vma()`: flush stale translations after clearing PTEs.
- `vmfault`: discarded anonymous pages should fault back through the demand-zero
  path from Lab 1.
- `uvmdiscard(pagetable, va, len)`: a good helper boundary for page-table work.

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
