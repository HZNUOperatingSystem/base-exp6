# Lab 3: Reuse Shared Clean File Pages

## Problem

A file-backed page has a stable source: file identity plus page offset.  If two
mappings read the same clean page, two physical copies waste memory.

Can the second mapping install its own PTE while reusing the first physical
page?

## Work

Add a tiny clean file-page cache in the kernel.

Use a key based on the underlying inode identity and page offset, not the file
descriptor number.  Track references so the physical page is freed only after
the last mapping goes away.

Run:

```sh
sharecheck
make grade STAGE=lab3
```

## Existing Interfaces You May Use

Useful interfaces and fields for shared clean file pages:

- `struct inode`, `ip->dev`, `ip->inum`: stable file identity for cache keys.
- `struct file`, `file->ip`: reach the inode behind a mapped file.
- `struct vma`, `vma->file`, `vma->file_offset`, `vma->addr`: connect a faulting
  virtual address back to file identity and offset.
- `PGROUNDDOWN`, `PGSIZE`: compute page-aligned file offsets.
- `struct spinlock`, `initlock`, `acquire`, `release`: protect a global cache.
- `walk`, `mappages`, `PTE_V`, `PTE_R`, `PTE_U`, `PTE2PA`: inspect and install
  PTEs, including two virtual mappings to one physical page.
- PTE flag definitions in `kernel/riscv/riscv.h`: inspect these if you decide to
  add a software bit that carries kernel-only metadata from fault time to unmap
  time.
- `kalloc()`, `kfree(pa)`, `memset`: allocate and clean up cache misses.
- `ilock`, `readi`, `iunlock`: fill a cache-miss page from the inode.
- `uvmunmap`: the release path that must distinguish shared file pages from
  ordinary pages.
- `panic(msg)`: useful for impossible internal cache invariants, not user input.
- Small helper functions are encouraged here, for example cache lookup, insert,
  release, and user-page release helpers.

## Hints

Sharing a physical page does not mean sharing a virtual address.  Each mapping
still needs its own PTE.

The mapped page should be read-only.  If user code can write a cached clean
file page, it is no longer safe to share as file contents.

For GDB, inspect the two PTEs after both reads.  Their virtual addresses should
differ; their physical page numbers should match.
