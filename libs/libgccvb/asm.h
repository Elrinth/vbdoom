#ifndef _LIBGCCVB_ASM_H
#define _LIBGCCVB_ASM_H


#define INT_ENABLE		asm("CLI;")
#define INT_DISABLE		asm("SEI;")

u32 jump_addr(void *addr);
extern void set_intlevel(u8 level);
extern int get_intlevel();


#endif