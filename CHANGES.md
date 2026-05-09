### User Library Cleanup and Debug Flow

2026-05-09, with [diff](https://github.com/utakotoba/xv6-riscv/compare/bb4759cc514da4616a84e5bb8bbe74893109ac56...2b9d0c7b56e34cb5972d57737d38df70c774a280)

Compared with [bb4759c](https://github.com/utakotoba/xv6-riscv/tree/bb4759cc514da4616a84e5bb8bbe74893109ac56), this update tidies userland reuse and improves debugging workflow:

- Extracted reusable user helpers into `ctype`, `io`, `math`, `term`, and `utf8` libraries.
- Slimmed `sh` and `llminfer` around those shared helpers while keeping shell and inference logic local.
- Added DEBUG-controlled QEMU GDB stub support, generated `.gdbinit`, and a `make debug` target.

### LLM Runtime, Larger Images, and Shell Polish

2026-05-09, with [diff](https://github.com/utakotoba/xv6-riscv/compare/f47d38ef288a48c3eb576df4e7c944cd7f3a690c...cf0fc2313ad636725ef6d059b1ec6aff19cec828)

Compared with [f47d38e](https://github.com/utakotoba/xv6-riscv/tree/f47d38ef288a48c3eb576df4e7c944cd7f3a690c), this update prepares the stripped base for larger file-backed demos and floating-point user programs:

- Added host-provided `files/` payload support for the filesystem image and a `make fast` shorthand for parallel builds.
- Extended the flat filesystem image for large files with double-indirect blocks, a larger block budget, and mkfs bitmap handling across multiple bitmap blocks.
- Added lazy user FPU support with per-process floating-point state, trap-time enablement, and RISC-V save/restore assembly.
- Added `llminfer`, a compact xv6-native Llama2 runner.
- Added `free` with a minimal `freemem` syscall for checking remaining memory from userland.
- Improved shell handling for quoted arguments, file completion after commands, and lower-flicker UTF-8-aware editing/redraw paths.

### Flat Read-Only Base and Shell UX

2026-05-08, with [diff](https://github.com/utakotoba/xv6-riscv/compare/418b737f431b80b69de4b94c17171e4f220ba629...d77b6a2863154972354a3787ce4d8fe10be9fcec)

Compared with [418b737](https://github.com/utakotoba/xv6-riscv/tree/418b737f431b80b69de4b94c17171e4f220ba629), this update tightens the base system around a flat, read-only runtime image:

- Stripped guest-side filesystem mutation: no create, truncate, link, unlink, mkdir, mknod, chdir, or pipe syscall surface.
- Reduced the kernel filesystem core to read-only lookup/read/stat behavior, removing pipe, log, inode allocation, truncation, directory insertion, and regular-file write paths.
- Adjusted `mkfs` to build the flat image from the host side, including `console` and only the selected user programs.
- Pruned extra user utilities and kept a compact shell-focused environment.
- Added `shutdown` as a user command backed by a minimal QEMU virt poweroff syscall path.
- Improved shell input with tab completion, command history, left/right cursor movement, edit-in-middle support, and UTF-8-aware redraw/delete handling.
- Made syscall stub generation tolerate sparse syscall numbers.

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
