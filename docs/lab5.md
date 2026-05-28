# Lab 5: Discard Sparse Heap Pages

## Goal

Add:

```c
int pagediscard(void *addr, uint64 len);
```

It should remove resident heap pages from a range without shrinking the
process's virtual size.

## Why

A large logical arena may contain many empty blocks.  The useful operation is
not always "make the address space smaller"; sometimes it is "forget these
physical pages, but keep the addresses valid for later."

## Work

Validate the range, unmap resident pages inside it, and leave holes that future
ordinary accesses can fault back in.

Run:

```sh
make grade STAGE=lab5
```

## Hints

Discard should not read or touch the discarded pages.  It should directly remove
mappings that already exist and ignore holes.

For GDB, step through the range validation first; most mistakes here are
off-by-one errors around page rounding.
