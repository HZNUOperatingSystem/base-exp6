# Lab 0: Ask The Kernel What Memory Looks Like

## Goal

Add one small observation syscall:

```c
int vmstat(struct vmstat *st);
```

`memtrace` should be able to ask the kernel for:

```c
struct vmstat {
  uint64 free_bytes;
  uint64 proc_size;
  uint64 page_size;
};
```

## Why

Before changing memory management, you need one trusted measurement path.  A
user program cannot safely read kernel allocator state or process metadata by
itself, so the first vertical slice is a syscall that copies a small struct from
the kernel to user space.

## Work

Fill in the `vmstat` syscall path.  Trace the existing syscall pattern from a
user declaration to `usys.pl`, `syscall.h`, `syscall.c`, and `sysproc.c`.

When it works:

```sh
make grade STAGE=lab0
```

## Hints

Use `copyout` to write the struct into the caller's address space.  The kernel
already has the current process and the free-memory helper.

For GDB, `ni` through the user `ecall`, then `si` into syscall dispatch.  If the
return value is wrong, inspect the syscall table entry before inspecting memory
accounting.
