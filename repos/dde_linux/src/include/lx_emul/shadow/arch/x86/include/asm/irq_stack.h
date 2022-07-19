/**
 * \brief  Shadow copy of asm/irq_stack.h
 * \author Stefan Kalkowski
 * \date   2022-06-29
 */

#ifndef _LX_EMUL__SHADOW__ARCH__X89__INCLUDE__ASM__IRQ_STACK_H_
#define _LX_EMUL__SHADOW__ARCH__X89__INCLUDE__ASM__IRQ_STACK_H_

#include_next <asm/irq_stack.h>

#undef call_on_stack
#undef ASM_CALL_ARG0

#define call_on_stack(stack, func, asm_call, argconstr...)		\
{									\
	register void *tos asm("r11");					\
									\
	tos = ((void *)(stack));					\
									\
	asm_inline volatile(						\
	"movq	%%rsp, (%[tos])				\n"		\
	"movq	%[tos], %%rsp				\n"		\
									\
	asm_call							\
									\
	"popq	%%rsp					\n"		\
									\
	: "+r" (tos), ASM_CALL_CONSTRAINT				\
	: [__func] "r" (func), [tos] "r" (tos) argconstr		\
	: "cc", "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",	\
	  "memory"							\
	);								\
}

#define ASM_CALL_ARG0							\
	"call *%P[__func]				\n"

#endif /* _LX_EMUL__SHADOW__ARCH__X89__INCLUDE__ASM__IRQ_STACK_H_ */
