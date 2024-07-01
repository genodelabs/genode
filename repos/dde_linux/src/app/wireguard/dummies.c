/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>


DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);

DEFINE_PER_CPU(unsigned long, cpu_scale);

#ifdef __aarch64__
DECLARE_BITMAP(system_cpucaps, ARM64_NCAPS);
EXPORT_SYMBOL(system_cpucaps);
#endif


/* mm/debug.c */
const struct trace_print_flags pagetype_names[] = {
	{0, NULL}
};


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


#include <net/ipv6_stubs.h>

const struct ipv6_stub *ipv6_stub = NULL;


#include <linux/netdevice.h>

int register_netdevice(struct net_device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <net/net_namespace.h>

int register_pernet_device(struct pernet_operations * ops)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <net/sock.h>

void sk_set_memalloc(struct sock * sk)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void synchronize_net(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_lock(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_unlock(void)
{
	lx_emul_trace(__func__);
}


#include <linux/netlink.h>

void do_trace_netlink_extack(const char * msg)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void napi_enable(struct napi_struct * n)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void napi_disable(struct napi_struct * n)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void __netif_napi_del(struct napi_struct * napi)
{
	lx_emul_trace(__func__);
}


#include <linux/mmzone.h>

struct mem_section ** mem_section = NULL;


unsigned long phys_base = 0;


#include <asm/atomic.h>

DEFINE_STATIC_KEY_FALSE(bpf_stats_enabled_key);


#include <linux/net.h>

int net_ratelimit(void)
{
	lx_emul_trace(__func__);
	return 0;
}


__wsum csum_partial(const void * buff,int len,__wsum sum)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

DEFINE_STATIC_KEY_FALSE(force_irqthreads_key);


#include <net/net_namespace.h>

void __init net_ns_init(void)
{
	lx_emul_trace(__func__);
}


unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n)

{
	lx_emul_trace_and_stop(__func__);
}


/* kernel/sched/cpudeadline.h */
struct cpudl;
int  cpudl_init(struct cpudl *cp)
{
	lx_emul_trace_and_stop(__func__);
	return -1;
}


void cpudl_cleanup(struct cpudl *cp)
{
	lx_emul_trace_and_stop(__func__);
}


/* kernel/sched/sched.h */
bool sched_smp_initialized = true;

struct dl_bw;
void init_dl_bw(struct dl_bw *dl_b)
{
	lx_emul_trace_and_stop(__func__);
}


struct irq_work;
extern void rto_push_irq_work_func(struct irq_work *work);
void rto_push_irq_work_func(struct irq_work *work)
{
	lx_emul_trace_and_stop(__func__);
}


/* include/linux/sched/topology.h */
int arch_asym_cpu_priority(int cpu)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


#ifdef CONFIG_ARM64
extern void flush_dcache_page(struct page * page);
void flush_dcache_page(struct page * page)
{
	lx_emul_trace(__func__);
}
#endif

pteval_t __default_kernel_pte_mask __read_mostly = ~0;


#include <linux/kernel.h>

bool parse_option_str(const char * str,const char * option)
{
	lx_emul_trace(__func__);
	return false;
}


int get_option(char ** str,int * pint)
{
	lx_emul_trace_and_stop(__func__);
}


#ifdef CONFIG_SWIOTLB
#include <linux/swiotlb.h>

bool is_swiotlb_allocated(void)
{
	lx_emul_trace(__func__);
	return false;
}
#endif
