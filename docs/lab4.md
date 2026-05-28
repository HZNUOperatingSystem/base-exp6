# Lab 4: Share Clean File Pages

## Goal

If the same process maps the same clean file page twice, the second mapping
should install a second virtual mapping but reuse the already loaded physical
page.

## Why

File-backed pages have a stable name: file identity plus page offset.  When the
contents are clean, two mappings do not need two physical copies.

## Work

Add a small kernel cache for clean file-backed pages.  Use inode identity and
page offset as the key.  Track references so the page is freed only after the
last mapping goes away.

Run:

```sh
make grade STAGE=lab4
```

## Hints

Sharing physical memory should not mean skipping the second PTE.  Each virtual
mapping still needs its own PTE and should still count as resident for that
address space.

For GDB, inspect the two PTEs after both accesses and compare their physical
page numbers.
