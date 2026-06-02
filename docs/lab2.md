# Lab 2: Map Model Bytes Instead Of Copying Them

## Problem

After demand-zero heap, `next` can get past the first allocation barrier.  The
next suspicious part is model loading: the file contains read-only weights, but
the runtime copies the whole weight region into anonymous heap memory.

Can a virtual range remember that its bytes come from a file, and load each page
only when the program first reads it?

## Work

Add a small read-only mapping syscall:

```c
char *filemap(int fd, uint64 len);
```

The syscall should reserve a virtual range and record enough metadata to serve
future faults from the file.  It should not copy the file contents eagerly.

On a page fault inside the range:

```text
allocate one page
read the matching file offset into it
map it read-only and user-accessible
resume the faulting load
```

Keep the file alive after `filemap` returns, even if the caller closes the file
descriptor.

Run:

```sh
mapcheck
make grade STAGE=lab2
```

## Hints

This is a deliberately small mapping interface, not full POSIX `mmap`.

Use a fixed-size VMA array in `struct proc`.  Start with one-page mappings if
that helps your first debug run, then handle multi-page lengths.

For GDB, compare `stval` with the recorded mapping range and inspect the file
offset used to fill the page.
