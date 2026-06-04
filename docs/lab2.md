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

You should also complete the `resident_pages` and `fault_count` in `struct vmstat`
for autograder to inspect.

Run:

```sh
mapcheck
make grade STAGE=lab2
```

## Existing Interfaces You May Use

Useful file, process, and VM interfaces already present in this tree:

- `struct proc`: holds per-process address-space state; this is a natural place
  to store mapping metadata.
- `struct file`: the kernel object behind an fd; it is more stable than the fd
  integer itself.
- `argfd(n, &fd, &f)`: validates a syscall fd argument and returns `struct file`.
- `argaddr(n, &x)`: reads an address-sized syscall argument such as a length.
- `filedup(f)`, `fileclose(f)`: hold and release references to an open file.
- `FD_INODE`, `f->readable`, `f->type`, `f->ip`: identify readable inode-backed
  files.
- `PGROUNDUP`, `PGROUNDDOWN`, `PGSIZE`, `TRAPFRAME`, `MAXVA`: page alignment and
  user address bounds.
- `ilock(ip)`, `readi(ip, user_dst, dst, off, n)`, `iunlock(ip)`: read bytes
  from an inode into a kernel page.
- `kalloc()`, `kfree(pa)`, `memset`: allocate, release, and initialize faulted
  pages.
- `walk`, `mappages`, `PTE_V`, `PTE_R`, `PTE_W`, `PTE_U`: inspect and install
  user PTEs.
- `uvmcopy`, `uvmunmap`, `freeproc`, `kfork`: lifecycle paths that must preserve
  or release per-process mapping metadata.
- `uvmresident(pagetable, sz)`: a useful metric helper for counting resident
  user pages.
- `vmfault(pagetable, va, read)`: the fault path where file metadata becomes a
  resident page.
- `copyout`: still needed when extending `vmstat` with resident/fault metrics.

## Hints

This is a deliberately small mapping interface, not full POSIX `mmap`.

Use a fixed-size VMA array in `struct proc`.  Start with one-page mappings if
that helps your first debug run, then handle multi-page lengths.

For GDB, compare `stval` with the recorded mapping range and inspect the file
offset used to fill the page.
