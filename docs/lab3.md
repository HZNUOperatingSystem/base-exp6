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

## Hints

Sharing a physical page does not mean sharing a virtual address.  Each mapping
still needs its own PTE.

The mapped page should be read-only.  If user code can write a cached clean
file page, it is no longer safe to share as file contents.

For GDB, inspect the two PTEs after both reads.  Their virtual addresses should
differ; their physical page numbers should match.
