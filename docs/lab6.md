# Lab 6: Drop Pages That Can Be Rebuilt

## Problem

Under memory pressure, not all resident pages are equally risky to evict.  A
clean file-backed page can be reconstructed from the file.  An anonymous page
cannot be dropped without losing data.

Can the kernel reclaim clean mapped file pages and fault them back later?

## Work

Implement:

```c
int pagereclaim(void);
```

For this lab, make it deterministic: reclaim clean file-backed pages from the
current process.  Keep the VMA metadata, remove resident PTEs, and free or
release the underlying physical pages correctly.

A later load from the same virtual address should fault the page back from the
file and return the same byte.

Run:

```sh
reclaimcheck
make grade STAGE=lab6
```

## Hints

Do not count reclaim itself as a page fault.  The second access is the refault.

If Lab 3 shared clean pages are present, reclaim must update the shared-page
reference count instead of freeing a page that another PTE still maps.

For GDB, use `ni` through `pagereclaim`, then `si` into the refault path and
compare the file offset with the first fault.
