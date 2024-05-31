/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Alexander Boettcher
 * \date   2022-07-01
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/debug.h>


DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/interrupt.h>

DEFINE_STATIC_KEY_FALSE(force_irqthreads_key);


#include <net/net_namespace.h>

void net_ns_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/skbuff.h>

void skb_init()
{
	lx_emul_trace(__func__);
}


extern int __init buses_init(void);
int __init buses_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init classes_init(void);
int __init classes_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init devices_init(void);
int __init devices_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/timekeeper_internal.h>

void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}


extern unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}
