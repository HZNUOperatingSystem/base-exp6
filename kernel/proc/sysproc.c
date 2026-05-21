#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "spinlock.h"
#include "types.h"
#include "vm.h"

uint64 sys_exit(void) {
    int n;
    argint(0, &n);
    kexit(n);
    return 0; // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return kfork(); }

uint64 sys_wait(void) {
    uint64 p;
    argaddr(0, &p);
    return kwait(p);
}

uint64 sys_sbrk(void) {
    int n;
    uint64 addr;

    argint(0, &n);
    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

uint64 sys_kill(void) {
    int pid;

    argint(0, &pid);
    return kkill(pid);
}

uint64 sys_shutdown(void) { poweroff(); }

uint64 sys_consolemode(void) {
    int raw;

    argint(0, &raw);
    consolesetraw(raw);
    return 0;
}

uint64 sys_freemem(void) { return kfreemem(); }
