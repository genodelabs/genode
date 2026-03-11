/*
 * \brief  Implementation of driver specific Linux functions
 * \author Sebastian Sumpf
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023-2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <usb_net.h>
#include <lx_emul.h>
#include <linux/net.h>

pteval_t __default_kernel_pte_mask __read_mostly = ~0;


struct usb_driver usbfs_driver = {
	.name = "usbfs"
};

const struct attribute_group *usb_device_groups[] = { };


#include <net/netfilter/nf_conntrack.h>

struct net init_net;


#include <net/net_namespace.h>

int register_pernet_subsys(struct pernet_operations *ops)
{
	if (ops->init)
		ops->init(&init_net);

	/* normally initialized in 'preinit_net()' */
	INIT_LIST_HEAD(&init_net.ptype_all);
	INIT_LIST_HEAD(&init_net.ptype_specific);

	return 0;
}


int register_pernet_device(struct pernet_operations *ops)
{
	return register_pernet_subsys(ops);
}


#include <linux/gfp.h>

unsigned long get_zeroed_page_noprof(gfp_t gfp_mask)
{
	return (unsigned long)kzalloc(PAGE_SIZE, GFP_KERNEL);
}


#include <linux/gfp.h>

void * __page_frag_alloc_align(struct page_frag_cache * nc, unsigned int fragsz,
                               gfp_t gfp_mask, unsigned int align_mask)
{
	if (align_mask != ~0U) {
		printk("page_frag_alloc_align: unsupported align_mask=%x\n", align_mask);
		lx_emul_trace_and_stop(__func__);
	}
	return lx_emul_mem_alloc_aligned(fragsz, ARCH_KMALLOC_MINALIGN);
}


void page_frag_free(void * addr)
{
	lx_emul_mem_free(addr);
}

#include <linux/uaccess.h>

#ifndef INLINE_COPY_TO_USER
unsigned long _copy_from_user(void * to,const void __user * from,unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


unsigned long raw_copy_from_user(void *to, const void * from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#else
unsigned long __must_check __arch_copy_from_user(void *to, const void __user *from, unsigned long n);
unsigned long __must_check __arch_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#endif


#ifndef INLINE_COPY_FROM_USER
unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


unsigned long raw_copy_to_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#else
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif


unsigned long arm_copy_from_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}



int netdev_register_kobject(struct net_device * ndev)
{
	return 0;
}


/*
 * QMI
 */
#define KBUILD_MODNAME "lx_emul"
#include <linux/usb/usbnet.h>

void usb_notify_add_device(struct usb_device * udev)
{
	/* handle set configuration */
	unsigned config = lx_emul_handle_config(dev_name(&udev->dev), udev->descriptor.idVendor,
	                                        udev->descriptor.idProduct);

	if (config && config <= udev->descriptor.bNumConfigurations &&
	    (!udev->actconfig || udev->actconfig->desc.bConfigurationValue != config)) {
		int err = usb_set_configuration(udev, config);
		if (err) printk("set configuration failed: %d\n", err);
	}

	/*
	 * Use usb_notify_add_device to search for interfaces claimed by the 'qmi_wwan'
	 * driver and enable raw-IP mode for these interfaces within 'qmi_wwan'
	 */
	unsigned interfaces = udev->actconfig->desc.bNumInterfaces;

	for (unsigned i = 0; i < interfaces; i++) {

		struct usb_interface *intf = usb_ifnum_to_if(udev, i);
		char const           *name = intf->dev.driver->name;

		if (strncmp(name, "qmi_wwan", 8) == 0) {
			struct usbnet *dev = usb_get_intfdata(intf);

			printk("found 'qmi_wwan' driver for interface %u\n", i);

			struct attribute *raw_ip = dev->net->sysfs_groups[0]->attrs[0];
			if (strncmp(raw_ip->name, "raw_ip", 6)) continue;

			printk("configuring raw-IP mode for 'qmi_wwan'\n");

			struct device_attribute *dev_attr = container_of(raw_ip,
			                                                 struct device_attribute,
			                                                 attr);
			char buf[] = { 'Y' };
			dev_attr->store(&dev->net->dev, NULL, buf, 1);
		}
	}
}


/*
 * USB-option driver
 */
#include "usb_option.h"

int nonseekable_open(struct inode *inode, struct file *filp)
{
	filp->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	return 0;
}


int fasync_helper(int, struct file *, int, struct fasync_struct **)
{
	return 1;
}


void kill_fasync(struct fasync_struct **, int, int) { }


void cdev_init(struct cdev *p, struct file_operations const *fops)
{
	memset(p, 0, sizeof(*p));
	INIT_LIST_HEAD(&p->list);
	p->ops = fops;
}


struct cdev *cdev_alloc(void)
{
	struct lx_option *option = kzalloc(sizeof(struct lx_option), GFP_KERNEL);
	if (option)
		INIT_LIST_HEAD(&option->cdev.list);

	return &option->cdev;
}


#ifndef USB_SERIAL_TTY_MAJOR
#define USB_SERIAL_TTY_MAJOR 188
#endif

int cdev_add(struct cdev *cdev, dev_t dev, unsigned count)
{
	struct lx_option *option = container_of(cdev, struct lx_option, cdev);

	option->cdev.dev   = dev;
	option->cdev.count = count;

	/* we only handle USB-serial ttys */
	if (MAJOR(dev) != USB_SERIAL_TTY_MAJOR) return 0;

	/*
	 * Do it here once, because we only want to initialize tty/terminal when USB-serial
	 * devices are present
	 */
	static bool once = true;
	if (once) {
		tty_init();
		n_tty_init();
		lx_option_init();
		once = false;
	}

	option->inode.i_rdev = dev;

	option->file.f_mode  = FMODE_READ | FMODE_WRITE | FMODE_CAN_READ |
	                       FMODE_CAN_WRITE | FMODE_STREAM;
	option->file.f_inode = &option->inode;
	option->dev = dev;

	struct file_operations const *fops = fops_get(option->cdev.ops);
	replace_fops(&option->file, fops);

	int err = option->file.f_op->open(&option->inode, &option->file);
	if (err < 0) return err;

	struct termios2 new_termios;
	option->file.f_op->unlocked_ioctl(&option->file, TCGETS2, (unsigned long)&new_termios);
	new_termios.c_cflag  &= ~CBAUD;
	new_termios.c_cflag  |= CBAUDEX;
	new_termios.c_lflag  &= ~(ECHO | ICANON | ISIG | IEXTEN);
	new_termios.c_ispeed  = 115200;
	new_termios.c_ospeed  = 115200;
	option->file.f_op->unlocked_ioctl(&option->file, TCSETS2, (unsigned long)&new_termios);

	/* create Terminal session */
	lx_option_create_session(option);

	return 0;
}


void cdev_del(struct cdev *cdev)
{
	struct lx_option *option = container_of(cdev, struct lx_option, cdev);

	if (MAJOR(option->dev) != USB_SERIAL_TTY_MAJOR) {
		kfree(option);
		return;
	}

	int err = option->file.f_op->release(&option->inode, &option->file);
	if (err) {
		printk("%s:%d: release failed %d\n", __func__, __LINE__, err);
		return;
	}

	lx_option_destroy_session(option);
}
