# Lab 3: File Contents As Lazy Page Contents

## Goal

Add a small file-backed mapping syscall:

```c
char *filemap(int fd, uint64 len);
```

The mapping should reserve address space immediately and load file data only
when the page is first read.

## Why

Heap faults create zero-filled pages.  This lab asks whether a fault can choose
a different source of contents: a file offset.

## Work

Store a small per-process mapping record.  On a fault inside that range,
allocate one page, read the right file bytes into it, and map it into the user
page table.

Run:

```sh
make grade STAGE=lab3
```

## Hints

Keep the file alive after the syscall returns.  A copied file descriptor is not
enough if the process closes the original descriptor before the fault.

For GDB, `si` into the page-fault branch and compare `stval` with the recorded
mapping base and length.
