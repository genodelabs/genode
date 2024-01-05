/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Sebastian Sumpf
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/interrupt.h>

DEFINE_STATIC_KEY_FALSE(force_irqthreads_key);

#ifdef __arm__
#include <asm/uaccess.h>

unsigned long arm_copy_to_user(void *to, const void *from, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}

asmlinkage void __div0(void);
asmlinkage void __div0(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-map-ops.h>

void arch_teardown_dma_ops(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern void arm_heavy_mb(void);
void arm_heavy_mb(void)
{
	// FIXME: on Cortex A9 we potentially need to flush L2-cache
	lx_emul_trace(__func__);
}

#else

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

#include <linux/timekeeper_internal.h>
void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}

#endif


DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);

unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n)

{
	lx_emul_trace_and_stop(__func__);
}


#include <net/net_namespace.h>

void net_ns_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/kobject.h>

int kobject_uevent(struct kobject * kobj,enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/fs.h>

int register_chrdev_region(dev_t from,unsigned count,const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

void __init usb_init_pool_max(void)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

void usb_hcd_synchronize_unlinks(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


#include <linux/refcount.h>

void refcount_warn_saturate(refcount_t * r,enum refcount_saturation_type t)
{
	lx_emul_trace(__func__);
}


#include <linux/semaphore.h>

int __sched down_interruptible(struct semaphore * sem)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int usb_major_init(void);
int usb_major_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init usb_devio_init(void);
int __init usb_devio_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/usb.h>

int usb_string(struct usb_device * dev,int index,char * buf,size_t size)
{
	lx_emul_trace(__func__);
	return 0;
}


extern char * usb_cache_string(struct usb_device * udev,int index);
char * usb_cache_string(struct usb_device * udev,int index)
{
	lx_emul_trace(__func__);
	return NULL;
}


void usb_kill_urb(struct urb * urb)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

struct usb_hcd * usb_get_hcd(struct usb_hcd * hcd)
{
	lx_emul_trace(__func__);
	return hcd;
}


#include <linux/usb/hcd.h>

void usb_put_hcd(struct usb_hcd * hcd)
{
	lx_emul_trace(__func__);
}


#include <linux/kernel.h>

bool parse_option_str(const char * str,const char * option)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/semaphore.h>

void __sched up(struct semaphore * sem)
{
	lx_emul_trace(__func__);
}


#include <linux/semaphore.h>

void __sched down(struct semaphore * sem)
{
	lx_emul_trace(__func__);
}


#include <linux/semaphore.h>

int __sched down_trylock(struct semaphore * sem)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/rcupdate.h>

void synchronize_rcu(void)
{
	lx_emul_trace(__func__);
}


#include <linux/input.h>

void input_ff_destroy(struct input_dev * dev)
{
	lx_emul_trace(__func__);
}


#include <linux/skbuff.h>

void skb_init()
{
	lx_emul_trace(__func__);
}


extern void software_node_notify(struct device * dev);
void software_node_notify(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern void software_node_notify_remove(struct device * dev);
void software_node_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern int usb_create_sysfs_dev_files(struct usb_device * udev);
int usb_create_sysfs_dev_files(struct usb_device * udev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void usb_remove_sysfs_dev_files(struct usb_device * udev);
void usb_remove_sysfs_dev_files(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


extern int usb_create_ep_devs(struct device * parent,struct usb_host_endpoint * endpoint,struct usb_device * udev);
int usb_create_ep_devs(struct device * parent,struct usb_host_endpoint * endpoint,struct usb_device * udev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void usb_remove_ep_devs(struct usb_host_endpoint * endpoint);
void usb_remove_ep_devs(struct usb_host_endpoint * endpoint)
{
	lx_emul_trace(__func__);
}


extern void usb_notify_add_device(struct usb_device * udev);
void usb_notify_add_device(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


extern void usb_notify_remove_device(struct usb_device * udev);
void usb_notify_remove_device(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


extern void usb_create_sysfs_intf_files(struct usb_interface * intf);
void usb_create_sysfs_intf_files(struct usb_interface * intf)
{
	lx_emul_trace(__func__);
}


extern void usb_remove_sysfs_intf_files(struct usb_interface * intf);
void usb_remove_sysfs_intf_files(struct usb_interface * intf)
{
	lx_emul_trace(__func__);
}


extern void usb_disable_interface(struct usb_device * dev,struct usb_interface * intf,bool reset_hardware);
void usb_disable_interface(struct usb_device * dev,struct usb_interface * intf,bool reset_hardware)
{
	lx_emul_trace(__func__);
}
