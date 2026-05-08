#ifndef XV6_PUBLIC_RISCV_H
#define XV6_PUBLIC_RISCV_H

#define PGSIZE 4096
#define PGSHIFT 12

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

#ifndef __ASSEMBLER__
#ifndef XV6_RISCV_R_SP
#define XV6_RISCV_R_SP
static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}
#endif
#endif

#endif
