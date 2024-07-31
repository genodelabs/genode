/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Josef Soentgen
 * \date   2022-02-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>


#include <asm/processor.h>
unsigned long __end_init_task[0];

#include <asm/current.h>
DEFINE_PER_CPU(struct pcpu_hot, pcpu_hot);

/* arch/x86/kernel/head64.c */
unsigned long vmalloc_base;

#include <asm-generic/sections.h>

char __start_rodata[] = {};
char __end_rodata[]   = {};

#include <asm/preempt.h>

int __preempt_count = 0;


#include <linux/prandom.h>

unsigned long net_rand_noise;


#include <linux/tracepoint-defs.h>

const struct trace_print_flags gfpflag_names[]  = { {0,NULL}};


#include <linux/tracepoint-defs.h>

const struct trace_print_flags vmaflag_names[]  = { {0,NULL}};


#include <linux/tracepoint-defs.h>

const struct trace_print_flags pageflag_names[] = { {0,NULL}};


/* mm/debug.c */
const struct trace_print_flags pagetype_names[] = {
	{0, NULL}
};


#include <asm/processor.h>

/*
 * Early_identify_cpu() in linux sets this up normally, used by drm_cache,
 * arch/x86/lib/delay.c, and slub allocator.
 */
struct cpuinfo_x86 boot_cpu_data =
{
    .x86_clflush_size    = (sizeof(void*) == 8) ? 64 : 32,
    .x86_cache_alignment = (sizeof(void*) == 8) ? 64 : 32,
    .x86_phys_bits       = (sizeof(void*) == 8) ? 36 : 32,
    .x86_virt_bits       = (sizeof(void*) == 8) ? 48 : 32
};

unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)];


/* kernel/sched/sched.h */
bool sched_smp_initialized = true;


/*
 * Generate_dummies.c will otherwise pull in <linux/rcutree.h>
 * that clashes with rcutiny.h.
 */
void rcu_barrier(void)
{
	lx_emul_trace(__func__);
}


#include <linux/cpuhotplug.h>

int __cpuhp_setup_state(enum cpuhp_state state,const char * name,bool invoke,int (* startup)(unsigned int cpu),int (* teardown)(unsigned int cpu),bool multi_instance)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/timekeeper_internal.h>

void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}


#include <linux/clocksource.h>

void clocksource_arch_init(struct clocksource * cs)
{
	lx_emul_trace(__func__);
}


#include <linux/sched/signal.h>

void ignore_signals(struct task_struct * t)
{
	lx_emul_trace(__func__);
}


#include <linux/kernel_stat.h>

void account_process_tick(struct task_struct * p,int user_tick)
{
	lx_emul_trace(__func__);
}


#include <linux/rcupdate.h>

void rcu_sched_clock_irq(int user)
{
	lx_emul_trace(__func__);
}


#include <linux/kernfs.h>

void kernfs_get(struct kernfs_node * kn)
{
	lx_emul_trace(__func__);
}


void kernfs_put(struct kernfs_node * kn)
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


void add_device_randomness(const void * buf, size_t size)
{
	lx_emul_trace(__func__);
}


#include <linux/random.h>

void add_interrupt_randomness(int irq)
{
	lx_emul_trace(__func__);
}


extern bool irq_wait_for_poll(struct irq_desc * desc);
bool irq_wait_for_poll(struct irq_desc * desc)
{
	lx_emul_trace_and_stop(__func__);
}



int register_chrdev_region(dev_t from,unsigned count,const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void register_handler_proc(unsigned int irq,struct irqaction * action);
void register_handler_proc(unsigned int irq,struct irqaction * action)
{
	lx_emul_trace(__func__);
}


extern void register_irq_proc(unsigned int irq,struct irq_desc * desc);
void register_irq_proc(unsigned int irq,struct irq_desc * desc)
{
	lx_emul_trace(__func__);
}


struct cdev;

int cdev_add(struct cdev * p,dev_t dev,unsigned count)
{
	lx_emul_trace(__func__);
	return 0;
}


void cdev_del(struct cdev * p)
{
	lx_emul_trace(__func__);
}


#include <linux/proc_fs.h>

struct proc_dir_entry { int dummy; };

struct proc_dir_entry * proc_create_seq_private(const char * name,umode_t mode,struct proc_dir_entry * parent,const struct seq_operations * ops,unsigned int state_size,void * data)
{
	static struct proc_dir_entry ret;
	lx_emul_trace(__func__);
	return &ret;
}


struct proc_dir_entry * proc_create_net_data(const char * name,umode_t mode,struct proc_dir_entry * parent,const struct seq_operations * ops,unsigned int state_size,void * data)
{
	static struct proc_dir_entry _proc_dir_entry;
	lx_emul_trace(__func__);
	return &_proc_dir_entry;
}


#include <linux/utsname.h>
#include <linux/user_namespace.h>

struct user_namespace init_user_ns;
struct uts_namespace init_uts_ns;


/*
 * linux/seq_file.h depends on user_namespace being defined, add
 * all dummies pulling in this header below here
 */


#include <linux/seq_file.h>

void seq_vprintf(struct seq_file * m,const char * f,va_list args)
{
	lx_emul_trace_and_stop(__func__);
}


extern void pci_allocate_vc_save_buffers(struct pci_dev * dev);
void pci_allocate_vc_save_buffers(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


extern void pci_vpd_init(struct pci_dev * dev);
void pci_vpd_init(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


extern int pci_proc_attach_device(struct pci_dev * dev);
int pci_proc_attach_device(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


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


#include <linux/pci.h>

void pci_fixup_device(enum pci_fixup_pass pass,struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


int pci_disable_link_state(struct pci_dev * pdev,int state)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int pci_dev_specific_acs_enabled(struct pci_dev * dev,u16 acs_flags);
int pci_dev_specific_acs_enabled(struct pci_dev * dev,u16 acs_flags)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int pci_dev_specific_disable_acs_redir(struct pci_dev * dev);
int pci_dev_specific_disable_acs_redir(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int pci_dev_specific_enable_acs(struct pci_dev * dev);
int pci_dev_specific_enable_acs(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int pci_dev_specific_reset(struct pci_dev * dev,int probe);
int pci_dev_specific_reset(struct pci_dev * dev,int probe)
{
	lx_emul_trace(__func__);
	return 0;
}


int pci_acpi_program_hp_params(struct pci_dev *dev)
{
    lx_emul_trace(__func__);
    return -ENODEV;
}


extern bool pat_enabled(void);
bool pat_enabled(void)
{
	/* used for mmap WC check */
	lx_emul_trace(__func__);
	return true;
}


struct srcu_struct;
extern int __srcu_read_lock(struct srcu_struct * ssp);
int __srcu_read_lock(struct srcu_struct * ssp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpu.h>

void cpu_hotplug_disable(void)
{
	lx_emul_trace(__func__);
}


#include <linux/cpu.h>

void cpu_hotplug_enable(void)
{
	lx_emul_trace(__func__);
}


extern void synchronize_srcu(struct srcu_struct * ssp);
void synchronize_srcu(struct srcu_struct * ssp)
{
	lx_emul_trace_and_stop(__func__);
}


#ifdef CONFIG_X86_64
DEFINE_PER_CPU(void *, hardirq_stack_ptr);
#endif
DEFINE_PER_CPU(bool, hardirq_stack_inuse);


#include <asm/processor.h>

DEFINE_PER_CPU_READ_MOSTLY(struct cpuinfo_x86, cpu_info);
EXPORT_PER_CPU_SYMBOL(cpu_info);


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


/* include/linux/sched/topology.h */
int arch_asym_cpu_priority(int cpu)
{
	lx_emul_trace_and_stop(__func__);
	return 0;
}


#include <linux/swiotlb.h>

void swiotlb_dev_init(struct device * dev)
{
	lx_emul_trace(__func__);
}


bool is_swiotlb_allocated(void)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/random.h>

int __cold execute_with_initialized_rng(struct notifier_block * nb)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sysctl.h>

struct ctl_table_header *register_sysctl_sz(const char *path, struct ctl_table *table,
                                            size_t table_size)
{
	lx_emul_trace(__func__);
	return NULL;
}

