#!/bin/sh

# list of prefixes
PREFIXES="
    riscv64-unknown-elf-
    riscv64-elf-
    riscv64-none-elf-
    riscv64-linux-gnu-
    riscv64-unknown-linux-gnu-
    riscv-none-elf-
"

# pick toolchain in order
for prefix in $PREFIXES; do
    if command -v "${prefix}gcc" >/dev/null 2>&1; then
        # ensure elf64 support
        if "${prefix}objdump" -i 2>&1 | grep -q 'elf64-littleriscv'; then
            echo "$prefix"
            exit 0
        fi
    fi
done

# output ERROR to be caught by CMake
echo "ERROR"
exit 1
