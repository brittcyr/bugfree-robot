/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];
extern struct Pseudodesc idt_pd;

void trap_init(void);
void trap_init_percpu(void);
void print_regs(struct PushRegs *regs);
void print_trapframe(struct Trapframe *tf);
void page_fault_handler(struct Trapframe *);
void backtrace(struct Trapframe *);

extern char divide[];
extern char debug[];
extern char nmi[];
extern char brkpt[];
extern char oflow[];
extern char bound[];
extern char illop[];
extern char device[];
extern char dblflt[];
extern char tss[];
extern char segnp[];
extern char stack[];
extern char gpflt[];
extern char pgflt[];
extern char fperr[];
extern char align[];
extern char mchk[];
extern char simderr[];

extern char _syscall[];
extern char _default[];

extern char irq_0[];
extern char irq_1[];
extern char irq_2[];
extern char irq_3[];
extern char irq_4[];
extern char irq_5[];
extern char irq_6[];
extern char irq_7[];
extern char irq_8[];
extern char irq_9[];
extern char irq_10[];
extern char irq_11[];
extern char irq_12[];
extern char irq_13[];
extern char irq_14[];
extern char irq_15[];

#endif /* JOS_KERN_TRAP_H */
