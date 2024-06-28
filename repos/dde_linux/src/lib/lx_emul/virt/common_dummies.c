/*
 * \brief  Common dummy definitions of Linux Kernel functions for virt_linux
 * \author Christian Helmuth
 * \date   2023-01-31
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#if defined(__x86_64__) || defined(__i386__)
#include <asm/processor.h>
unsigned long __end_init_task[0];

#include <asm/current.h>
DEFINE_PER_CPU(struct pcpu_hot, pcpu_hot);
#endif


#include <asm-generic/sections.h>

char __start_rodata[] = {};
char __end_rodata[]   = {};

extern int __init platform_bus_init(void);
int __init platform_bus_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/kernel_stat.h>

void account_process_tick(struct task_struct * p,int user_tick)
{
	lx_emul_trace(__func__);
}


#include <linux/random.h>
struct random_ready_callback;

int add_random_ready_callback(struct random_ready_callback * rdy)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <linux/interrupt.h>

int __init early_irq_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/irq.h>
#include <linux/irqdesc.h>

int generic_handle_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpuhotplug.h>

int __cpuhp_setup_state(enum cpuhp_state state,const char * name,bool invoke,int (* startup)(unsigned int cpu),int (* teardown)(unsigned int cpu),bool multi_instance)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <asm/irq_regs.h>
struct pt_regs * __irq_regs = NULL;


#include <asm/preempt.h>

int __preempt_count = 0;


#include <linux/prandom.h>

unsigned long net_rand_noise;


#include <linux/tracepoint-defs.h>

const struct trace_print_flags gfpflag_names[]  = { {0,NULL}};


#include <linux/tracepoint-defs.h>

const struct trace_print_flags pageflag_names[] = { {0,NULL}};


#include <linux/tracepoint-defs.h>

const struct trace_print_flags vmaflag_names[]  = { {0,NULL}};


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>

void rcu_barrier(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rcupdate.h>

void rcu_sched_clock_irq(int user)
{
	lx_emul_trace(__func__);
}


#include <linux/sched/signal.h>

void ignore_signals(struct task_struct * t)
{
	lx_emul_trace(__func__);
}


#ifdef CONFIG_TREE_SRCU
#include <linux/srcu.h>

void synchronize_srcu(struct srcu_struct * ssp)
{
	lx_emul_trace_and_stop(__func__);
}
#endif
