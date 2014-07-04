#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
    
    /* For each, I am setting the gate to be the correct index into idt,
       the istrap to 0 because they are not traps, sel to GD_KT, off to
       the name of the handler and dpl to 3 since we are making the kernel 
       handle everything
    */
    SETGATE(idt[T_DIVIDE], 0, GD_KT, divide, 3);
    SETGATE(idt[T_DEBUG], 0, GD_KT, debug, 3);
    SETGATE(idt[T_NMI], 0, GD_KT, nmi, 3);
    SETGATE(idt[T_BRKPT], 0, GD_KT, brkpt, 3);
    SETGATE(idt[T_OFLOW], 0, GD_KT, oflow, 3);
    SETGATE(idt[T_BOUND], 0, GD_KT, bound, 3);
    SETGATE(idt[T_ILLOP], 0, GD_KT, illop, 3);
    SETGATE(idt[T_DEVICE], 0, GD_KT, device, 3);
    SETGATE(idt[T_DBLFLT], 0, GD_KT, dblflt, 3);
    SETGATE(idt[T_TSS], 0, GD_KT, tss, 3);
    SETGATE(idt[T_SEGNP], 0, GD_KT, segnp, 3);
    SETGATE(idt[T_STACK], 0, GD_KT, stack, 3);
    SETGATE(idt[T_GPFLT], 0, GD_KT, gpflt, 3);
    SETGATE(idt[T_PGFLT], 0, GD_KT, pgflt, 0);
    SETGATE(idt[T_FPERR], 0, GD_KT, fperr, 3);
    SETGATE(idt[T_ALIGN], 0, GD_KT, align, 3);
    SETGATE(idt[T_MCHK], 0, GD_KT, mchk, 3);
    SETGATE(idt[T_SIMDERR], 0, GD_KT, simderr, 3);

    SETGATE(idt[T_SYSCALL], 0, GD_KT, _syscall, 3);
    SETGATE(idt[T_DEFAULT], 0, GD_KT, _default, 3);

    SETGATE(idt[IRQ_OFFSET], 0, GD_KT, irq_0, 3);
    SETGATE(idt[IRQ_OFFSET + 1], 0, GD_KT, irq_1, 0);
    SETGATE(idt[IRQ_OFFSET + 2], 0, GD_KT, irq_2, 0);
    SETGATE(idt[IRQ_OFFSET + 3], 0, GD_KT, irq_3, 0);
    SETGATE(idt[IRQ_OFFSET + 4], 0, GD_KT, irq_4, 0);
    SETGATE(idt[IRQ_OFFSET + 5], 0, GD_KT, irq_5, 0);
    SETGATE(idt[IRQ_OFFSET + 6], 0, GD_KT, irq_6, 0);
    SETGATE(idt[IRQ_OFFSET + 7], 0, GD_KT, irq_7, 0);
    SETGATE(idt[IRQ_OFFSET + 8], 0, GD_KT, irq_8, 0);
    SETGATE(idt[IRQ_OFFSET + 9], 0, GD_KT, irq_9, 0);
    SETGATE(idt[IRQ_OFFSET + 10], 0, GD_KT, irq_10, 0);
    SETGATE(idt[IRQ_OFFSET + 11], 0, GD_KT, irq_11, 0);
    SETGATE(idt[IRQ_OFFSET + 12], 0, GD_KT, irq_12, 0);
    SETGATE(idt[IRQ_OFFSET + 13], 0, GD_KT, irq_13, 0);
    SETGATE(idt[IRQ_OFFSET + 14], 0, GD_KT, irq_14, 0);
    SETGATE(idt[IRQ_OFFSET + 15], 0, GD_KT, irq_15, 0);

	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.

    // Check if it is a page fault exception
    if (tf->tf_trapno == T_PGFLT) {
        // If it is, call page_fault_handler()
        page_fault_handler(tf);
        return;
    }

    // Check if it is a breakpoint exception
    if (tf->tf_trapno == T_BRKPT) {
        // If it is, call page_fault_handler()
        monitor(tf);
        return;
    }

    // Check if it is a SYSCALL
    if (tf->tf_trapno == T_SYSCALL) {
        // If it is, call syscall() and put result it eax
        tf->tf_regs.reg_eax = syscall(
                tf->tf_regs.reg_eax,
                tf->tf_regs.reg_edx,
                tf->tf_regs.reg_ecx,
                tf->tf_regs.reg_ebx,
                tf->tf_regs.reg_edi,
                tf->tf_regs.reg_esi);
        return;
    }

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	cprintf("Incoming TRAP frame at %p\n", tf);

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		assert(curenv);

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// Return to the current environment, which should be running.
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

    // Check the lower two bytes
    if ((tf->tf_cs & 0xFF) == GD_KT) {
        panic("Page fault in kernel mode");
    }

	// LAB 3: Your code here.

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}
