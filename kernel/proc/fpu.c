#include "defs.h"
#include "proc.h"
#include "riscv.h"
#include "types.h"

extern void fpu_save_regs(uint64*);
extern void fpu_restore_regs(uint64*);

void fpu_save_proc(struct proc* p) {
    if (p && p->fpu_active) {
        fpu_on();
        fpu_save_regs(p->fpu.f);
        p->fpu.fcsr = r_fcsr();
        p->fpu_active = 0;
    }
    fpu_off();
}

void fpu_handle_trap(struct proc* p) {
    fpu_on();
    if (!p->fpu_used) {
        memset(&p->fpu, 0, sizeof(p->fpu));
        p->fpu_used = 1;
    }
    fpu_restore_regs(p->fpu.f);
    w_fcsr(p->fpu.fcsr);
    p->fpu_active = 1;
}
