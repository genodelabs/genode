/*
 * \brief  Dummy definitions of Linux kernel functions
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

unsigned smp_on_up;


/* lx_kit's 'kernel_init' in start.c */

extern int __init devices_init(void);
int __init devices_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init buses_init(void);
int __init buses_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/jump_label.h>
#include <asm/processor.h>

struct static_key_false init_on_alloc;

unsigned long sysctl_net_busy_read;

DEFINE_STATIC_KEY_FALSE(force_irqthreads_key);
DEFINE_STATIC_KEY_FALSE(bpf_stats_enabled_key);


#include <linux/cpumask.h>
#include <linux/percpu-defs.h>

DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);


#include <net/ipv6_stubs.h>

const struct ipv6_stub *ipv6_stub = NULL;


#include <asm/uaccess.h>

long strncpy_from_user(char * dst,const char __user * src,long count)
{
	lx_emul_trace_and_stop(__func__);
}


int __copy_from_user_flushcache(void *dst, const void __user *src, unsigned size)
{
	lx_emul_trace_and_stop(__func__);
}

#ifdef __x86_64__

unsigned long clear_user(void *mem, unsigned long len)
{
	lx_emul_trace_and_stop(__func__);
}

#endif

#ifdef ARCH_HAS_NOCACHE_UACCESS

int __copy_from_user_inatomic_nocache(void *dst, const void __user *src,
                                      unsigned size)
{
 	lx_emul_trace_and_stop(__func__);
}

#endif


#include <asm/uaccess.h>

long strnlen_user(const char __user * str,long count)
{
	lx_emul_trace_and_stop(__func__);
}


void add_device_randomness(const void * buf,size_t len)
{
	lx_emul_trace(__func__);
}


#include <linux/device.h>

void *__devres_alloc_node(dr_release_t release, size_t size, gfp_t gfp,
                          int nid, const char *name)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


void devres_free(void *res)
{
	lx_emul_trace_and_stop(__func__);
}


void devres_add(struct device *dev, void *res)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcutree.h>

void synchronize_rcu_expedited(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rcupdate.h>

void synchronize_rcu(void)
{
	lx_emul_trace(__func__);
}


#include <linux/kernel.h>

char *get_options(const char *str, int nints, int *ints)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


#include <linux/fs.h>

char *file_path(struct file *, char *, int)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


#include <asm/page.h>

#ifdef __x86_64__

void copy_page(void *to, void *from)
{
	lx_emul_trace_and_stop(__func__);
}

#endif


#include <linux/filter.h>

void bpf_jit_compile(struct bpf_prog *prog)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/bpf.h>

u64 bpf_get_raw_cpu_id(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


#ifndef __arm__

#include <linux/timekeeper_internal.h>
void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}

#else

#include <asm/cacheflush.h>

struct cpu_cache_fns cpu_cache;


#include <asm/uaccess.h>

asmlinkage void __div0(void);
asmlinkage void __div0(void)
{
	lx_emul_trace_and_stop(__func__);
}

#endif


#include <linux/kobject.h>

int kobject_uevent(struct kobject * kobj,enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/ethtool.h>

int ethtool_check_ops(const struct ethtool_ops * ops)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/device/driver.h>

void wait_for_device_probe(void)
{
	lx_emul_trace(__func__);
}


#include <linux/irq_work.h>

void irq_work_tick(void)
{
	lx_emul_trace(__func__);
}


#include <linux/pid.h>

void put_pid(struct pid * pid)
{
	lx_emul_trace(__func__);
}


#include <linux/ratelimit_types.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <asm-generic/softirq_stack.h>

void do_softirq_own_stack(void)
{
	lx_emul_trace(__func__);
}


unsigned long __must_check __arch_clear_user(void __user *to, unsigned long n);
unsigned long __must_check __arch_clear_user(void __user *to, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


unsigned long __must_check arm_clear_user(void __user *addr, unsigned long n);
unsigned long __must_check arm_clear_user(void __user *addr, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


u64 bpf_user_rnd_u32(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


u64 bpf_event_output(struct bpf_map *map, u64 flags, void *meta, u64 meta_size,
                     void *ctx, u64 ctx_size, bpf_ctx_copy_t ctx_copy)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
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


void dl_clear_root_domain(struct root_domain *rd)
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


extern int __init netdev_kobject_init(void);
int __init netdev_kobject_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int netdev_register_kobject(struct net_device * ndev);
int netdev_register_kobject(struct net_device * ndev)
{
	lx_emul_trace(__func__);
	return 0;
}
