# Lab 0: Explain The First Failure

## Problem

Start with the workload, not the kernel:

```sh
make download-files
make emulate
next model.bin tokenizer.bin 1 "hello"
```

On the starter system, `next` should fail before it can print a token.  The
failure is usually just `malloc failed`, which is not enough evidence to debug
an operating-system problem.

Your task is to add the smallest trusted observation path from the kernel to a
user program:

```c
int vmstat(struct vmstat *st);
```

`memwhy model.bin` should then report memory facts that help explain why eager
allocation cannot fit comfortably in a 64 MiB guest.

## Work

Implement the `vmstat` syscall path:

```text
include/vm.h
include/syscall.h
user/include/user.h
scripts/gen_usys.sh output
kernel/proc/syscall.c
kernel/proc/sysproc.c
```

For this lab, fill:

```c
free_bytes
proc_size
page_size
```

The remaining fields may be zero for now.

Run:

```sh
memwhy model.bin
make grade STAGE=lab0
```

## Existing Interfaces You May Use

Useful kernel/user interfaces already present in this tree:

- `struct vmstat` in `include/vm.h`: the shared result layout copied from kernel
  to user space.
- `struct proc` in `kernel/include/proc.h`: the current process record;
  `p->sz` is its virtual size and `p->pagetable` is its user page table.
- `myproc()`: returns the process currently running this syscall.
- `argaddr(n, &x)`: reads syscall argument `n` as an address-sized value.
- `copyout(pagetable, dstva, src, len)`: copies a kernel buffer to a user
  virtual address.
- `kfreemem()`: reports free physical memory in bytes.
- `memset(ptr, value, n)`: clears or fills a kernel buffer before copying it.
- `PGSIZE`: the hardware page size used by xv6.
- `SYS_*` numbers in `include/syscall.h`: syscall ABI numbers.
- `sys_*` handlers in `kernel/proc/sysproc.c`: kernel-side syscall bodies.
- `scripts/gen_usys.sh`: generates the user syscall stubs from the syscall list.

## Hints

`freemem` already exposes one memory fact.  `vmstat` is different because it
copies a structured snapshot, including current process state, to a user
pointer.

Use `copyout`; do not cast the user pointer and write through it directly.

For GDB, use `ni` over the user `ecall`, then `si` into syscall dispatch.  If
the syscall returns `-1`, inspect the syscall table entry before inspecting the
memory fields.
