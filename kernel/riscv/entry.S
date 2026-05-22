    # qemu -kernel loads the kernel at 0x80000000
    # and causes each hart (i.e. CPU) to jump there.
    # kernel.ld causes the following code to
    # be placed at 0x80000000.
.section .text.entry
.global _entry
_entry:
    # qemu passes hartid in a0 and DTB address in a1.
    # save DTB before we clobber a1 with mhartid.
    mv s0, a1
    # set up a stack for C.
    # stack0 is declared in start.c,
    # with a 4096-byte stack per CPU.
    # sp = stack0 + ((hartid + 1) * 4096)
    la sp, stack0
    li a0, 1024*4
    csrr a1, mhartid
    addi a1, a1, 1
    mul a0, a0, a1
    add sp, sp, a0
    # pass DTB address as first argument to start()
    mv a0, s0
    call start
spin:
    j spin
