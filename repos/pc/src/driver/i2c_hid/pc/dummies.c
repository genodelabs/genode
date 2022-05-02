/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Christian Helmuth
 * \date   2022-05-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/pci.h>

const struct attribute_group pci_dev_acpi_attr_group;

pteval_t __default_kernel_pte_mask __read_mostly = ~0;

#include <linux/seq_file.h>

void seq_printf(struct seq_file * m,const char * f,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int __printk_ratelimit(const char * func)
{
	lx_emul_trace(__func__);
	/* suppress */
	return 0;
}

#include <linux/kobject.h>

int kobject_uevent(struct kobject * kobj,enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}

#include <linux/proc_fs.h>

struct proc_dir_entry * proc_mkdir(const char * name,struct proc_dir_entry * parent)
{
	static struct { void *p; } dummy;
	lx_emul_trace(__func__);
	return (struct proc_dir_entry *)&dummy;
}

#include <linux/proc_fs.h>

struct proc_dir_entry * proc_create(const char * name,umode_t mode,struct proc_dir_entry * parent,const struct proc_ops * proc_ops)
{
	static struct { void *p; } dummy;
	lx_emul_trace(__func__);
	return (struct proc_dir_entry *)&dummy;
}

#include <linux/pci.h>

struct irq_domain *pci_host_bridge_acpi_msi_domain(struct pci_bus *bus)
{
	lx_emul_trace(__func__);
	return NULL;
}


int pci_alloc_irq_vectors(struct pci_dev * dev,unsigned int min_vecs,unsigned int max_vecs,unsigned int flags)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/fs.h>

int alloc_chrdev_region(dev_t * dev, unsigned baseminor, unsigned count, const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <linux/acpi.h>

void acpi_device_notify(struct device * dev)
{
	lx_emul_trace(__func__);
}

extern bool dev_add_physical_location(struct device * dev);
bool dev_add_physical_location(struct device * dev)
{
	lx_emul_trace(__func__);
	return false;
}

#include <linux/acpi.h>

int acpi_check_resource_conflict(const struct resource * res)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <acpi/acpi_bus.h>

int acpi_device_fix_up_power(struct acpi_device * device)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <linux/sched/rt.h>

void rt_mutex_setprio(struct task_struct *p, struct task_struct *pi_task)
{
	lx_emul_trace(__func__);
}

#include <linux/input.h>

void input_ff_destroy(struct input_dev * dev)
{
	lx_emul_trace(__func__);
}

#include <linux/input.h>

int input_ff_event(struct input_dev * dev,unsigned int type,unsigned int code,int value)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <linux/sysfs.h>

const struct attribute_group dev_attr_physical_location_group = {};

#include <linux/sysfs.h>

int sysfs_add_file_to_group(struct kobject * kobj,const struct attribute * attr,const char * group)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <linux/percpu-defs.h>

DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);

#include <linux/context_tracking_irq.h>

noinstr void ct_irq_enter(void)
{
	lx_emul_trace(__func__);
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_exit(void)
{
	lx_emul_trace(__func__);
}

#include <linux/clk-provider.h>

void clk_unregister(struct clk * clk)
{
	lx_emul_trace_and_stop(__func__);
}

#include <linux/clk-provider.h>

int clk_hw_register(struct device * dev, struct clk_hw * hw)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <linux/sysctl.h>

void __init __register_sysctl_init(const char * path,struct ctl_table * table,const char * table_name,size_t table_size)
{
	lx_emul_trace(__func__);
}

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

#include <linux/iommu.h>

int iommu_device_use_default_domain(struct device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}

void iommu_device_unuse_default_domain(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <asm/smp.h>

struct smp_ops smp_ops = { };


#include <drm/drm_panel.h>

bool drm_is_panel_follower(struct device * dev)
{
	return false;
}


#include <linux/srcu.h>

void __srcu_read_unlock(struct srcu_struct * ssp,int idx)
{
	lx_emul_trace(__func__);
}


int init_srcu_struct(struct srcu_struct * ssp)
{
	lx_emul_trace(__func__);
	return 0;
}
