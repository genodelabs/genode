/*
 * \brief  Implementation of driver specific Linux functions
 * \author Christian Helmuth
 * \date   2025-09-17
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/usb.h>
#include <linux/tty.h>

#include "lx_emul.h"


struct usb_driver usbfs_driver = {
	.name = "usbfs"
};


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_backtrace();
}


#include <linux/percpu.h>

DEFINE_PER_CPU(unsigned long, cpu_scale);

const struct attribute_group *usb_device_groups[] = { };

void lx_emul_usb_client_device_unregister_callback(struct usb_device *udev) { }


#ifdef __i386__
#include <asm/fixmap.h>
unsigned long __FIXADDR_TOP = 0xfffff000;
#endif


#include <asm/uaccess.h>

#ifndef INLINE_COPY_FROM_USER
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


#ifndef INLINE_COPY_TO_USER
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


#include <linux/fcntl.h>

int fasync_helper(int, struct file *, int, struct fasync_struct **)
{
	return 1;
}


void kill_fasync(struct fasync_struct **, int, int) { }


#include <linux/tty_driver.h>
#include <linux/tty_port.h>
#include <linux/poll.h>

static struct cdev *cdev;

struct cdev *cdev_alloc(void)
{
	struct cdev *p = kzalloc(sizeof(struct cdev), GFP_KERNEL);
	if (p)
		INIT_LIST_HEAD(&p->list);

	return p;
}


int cdev_add(struct cdev *p, dev_t dev, unsigned count)
{
	p->dev = dev;
	p->count = count;

	cdev = p;

	return 0;
}


void cdev_del(struct cdev *p)
{
	/* XXX dynamics are not supported yet - see lx_emul_usb_serial_write() */
}


void cdev_init(struct cdev *p, struct file_operations const *fops)
{
	memset(p, 0, sizeof(*p));
	INIT_LIST_HEAD(&p->list);
	p->ops = fops;
}


int nonseekable_open(struct inode *inode, struct file *filp)
{
	filp->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	return 0;
}


static struct file * cdev_file(void)
{
	if (!cdev) return NULL;

	static struct inode *inode;

	if (!inode) {
		inode = kzalloc(sizeof(*inode), GFP_KERNEL);
		inode->i_rdev = MKDEV(188, 0);
	}

	static struct file *file;

	if (!file) {
		file = kzalloc(sizeof(*file), GFP_KERNEL);

		file->f_mode  = FMODE_READ | FMODE_WRITE | FMODE_CAN_READ | FMODE_CAN_WRITE | FMODE_STREAM;
		file->f_inode = inode;

		struct file_operations const *fops = fops_get(cdev->ops);
		replace_fops(file, fops);

		if (file->f_op->open(inode, file) < 0) {
			kfree(file);
			file = NULL;
		} else {
			struct termios2 new_termios;
			file->f_op->unlocked_ioctl(file, TCGETS2, (unsigned long)&new_termios);
			new_termios.c_cflag  &= ~CBAUD;
			new_termios.c_cflag  |= CBAUDEX;
			new_termios.c_lflag  &= ~(ECHO | ICANON | ISIG | IEXTEN);
			new_termios.c_ispeed  = 115200;
			new_termios.c_ospeed  = 115200;
			file->f_op->unlocked_ioctl(file, TCSETS2, (unsigned long)&new_termios);
		}
	}

	return file;
}


void lx_emul_usb_serial_wait_for_device()
{
	while (!cdev_file())
		msleep(1000);
}


void lx_emul_usb_serial_write(struct genode_const_buffer buffer)
{
	struct file *file = cdev_file();
	if (!file) return;

	loff_t tmp = 0;
	kernel_write(file, buffer.start, buffer.num_bytes, &tmp);
}


unsigned long lx_emul_usb_serial_read(struct genode_buffer buffer)
{
	struct file *file = cdev_file();
	if (!file) return 0;

	if (!(vfs_poll(file, NULL) & EPOLLIN)) return 0;

	loff_t tmp = 0;
	ssize_t ret = kernel_read(file, buffer.start, buffer.num_bytes, &tmp);

	return ret > 0 ? ret : 0;
}
