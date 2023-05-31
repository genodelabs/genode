/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Josef Soentgen
 * \date   2022-01-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

const struct attribute_group pci_dev_acpi_attr_group;


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/property.h>

struct device_driver;


struct kobj_uevent_env;


struct pci_dev;
bool pciehp_is_native(struct pci_dev *bridge)
{
	lx_emul_trace_and_stop(__func__);
}

#include <linux/pci.h>

struct irq_domain *pci_host_bridge_acpi_msi_domain(struct pci_bus *bus)
{
	lx_emul_trace(__func__);
	return NULL;
}


int usb_acpi_register(void)
{
	lx_emul_trace(__func__);
	return 0;
};


void usb_acpi_unregister(void)
{
	lx_emul_trace(__func__);
};


struct usb_device;
bool usb_acpi_power_manageable(struct usb_device *hdev, int index)
{
	lx_emul_trace(__func__);
	return false;
}


int usb_acpi_set_power_state(struct usb_device *hdev, int index, bool enable)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/printk.h>

int __printk_ratelimit(const char * func)
{
	lx_emul_trace(__func__);
	/* suppress */
	return 0;
}


#include <linux/pci.h>

void pci_disable_device(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


#include <asm/smp.h>

DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);


const struct attribute_group dev_attr_physical_location_group = {};


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


#include <linux/sysctl.h>

struct ctl_table_header * register_sysctl(const char * path,struct ctl_table * table)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <linux/iommu.h>

int iommu_device_use_default_domain(struct device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/usb.h>

int usb_acpi_port_lpm_incapable(struct usb_device * hdev,int index)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sysctl.h>

void __init __register_sysctl_init(const char * path,struct ctl_table * table,const char * table_name)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

int sysfs_add_file_to_group(struct kobject * kobj,const struct attribute * attr,const char * group)
{
	lx_emul_trace(__func__);
	return 0;
}


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


#include <linux/acpi.h>

void acpi_device_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern void software_node_notify_remove(struct device * dev);
void software_node_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <net/net_namespace.h>

void net_ns_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/fs.h>

struct timespec64 current_time(struct inode * inode)
{
	struct timespec64 ret = { 0 };
	lx_emul_trace(__func__);
	return ret;
}


#include <linux/pid.h>

void put_pid(struct pid * pid)
{
	lx_emul_trace(__func__);
}


#include <linux/cred.h>

void __put_cred(struct cred * cred)
{
	lx_emul_trace(__func__);
}


#include <linux/skbuff.h>

void skb_init()
{
	lx_emul_trace(__func__);
}
