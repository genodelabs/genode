/*
 * \brief  Shadow copy of asm/current.h
 * \author Sebastian Sumpf
 * \date   2024-06-26
 */

#ifndef _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__CURRENT_H_
#define _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__CURRENT_H_

#include <linux/compiler.h>

#ifndef __ASSEMBLY__

#include <linux/build_bug.h>
#include <linux/version.h>

#include_next <asm/current.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
struct task_struct;

struct pcpu_hot {
	union {
		struct {
			struct task_struct	*current_task;
			int			preempt_count;
			int			cpu_number;
#ifdef CONFIG_CALL_DEPTH_TRACKING
			u64			call_depth;
#endif
			unsigned long		top_of_stack;
			void			*hardirq_stack_ptr;
			u16			softirq_pending;
#ifdef CONFIG_X86_64
			bool			hardirq_stack_inuse;
#else
			void			*softirq_stack_ptr;
#endif
		};
		u8	pad[64];
	};
};

static_assert(sizeof(struct pcpu_hot) == 64);

extern struct pcpu_hot pcpu_hot;

#endif /* LINUX_VERSION_CODE */

#endif /* __ASSEMBLY__ */

#endif /*_LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__CURRENT_H_ */
