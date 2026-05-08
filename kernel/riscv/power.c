#include "defs.h"
#include "memlayout.h"
#include "riscv.h"
#include "types.h"

#define VIRT_TEST_PASS 0x5555

void poweroff(void) {
    intr_off();
    *(volatile uint32*)VIRT_TEST = VIRT_TEST_PASS;
    for (;;)
        asm volatile("wfi");
}
