### Refactor, Strip, and User-Friendly Optimizations

2026-05-08, with [diff](https://github.com/utakotoba/xv6-riscv/compare/5474d4bf72fd95a6e5c735c2d7f208f58990ceab...38eb6bb37dec65d08216cb14286f7f4ff20437eb)

Compared with upstream [xv6-riscv@5474d4b](https://github.com/mit-pdos/xv6-riscv/tree/5474d4bf72fd95a6e5c735c2d7f208f58990ceab), this fork focuses on a smaller, cleaner base for further work:

- Reorganized headers into public `include/`, kernel-private `kernel/include/`, and user-private `user/include/`.
- Grouped kernel sources by responsibility under `kernel/core`, `kernel/dev`, `kernel/fs`, `kernel/mm`, `kernel/proc`, `kernel/sync`, and `kernel/riscv`.
- Split userland into shell-runnable programs in `user/bin` and support code in `user/lib`.
- Removed xv6 course/test/demo user programs, keeping a compact baseline shell environment.
- Moved linker scripts to `linker/` and host tooling to `tools/`.
- Reworked the Makefile around wildcard discovery, out-of-tree build artifacts, QEMU configuration, and cleaner build output.
- Replaced the hardcoded syscall stub generator with `scripts/gen_usys.sh`, derived from `include/syscall.h`.
- Split kernel ELF `LOAD` segments in `linker/kernel.ld` so text, rodata, and data/bss have separate `R-X`, `R--`, and `RW-` permissions.
