# Lab 6: Reclaim Clean File Pages

## Goal

Add:

```c
int pagereclaim(void);
```

For this lab, reclaim clean file-backed pages from the current process.  A later
read should fault the page back from the file.

## Why

The kernel can drop some pages more safely than others.  A clean file-backed
page has a backing source, so it can leave memory without losing data.

## Work

Find clean file-backed resident pages, unmap them, and keep enough mapping
metadata so the next access reloads the file page correctly.

Run:

```sh
make grade STAGE=lab6
```

## Hints

Do not count reclaim itself as a demand fault.  The fault happens when the
process later reads the reclaimed address.

For GDB, disassemble around `sepc` after the refault if the byte changes or the
process exits unexpectedly.
