/*
 * \brief  Post kernel activity
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/sched/task.h>
#include <linux/usb.h>
#include <lx_emul/nic.h>
#include <lx_user/init.h>
#include <genode_c_api/uplink.h>
#include <genode_c_api/mac_address_reporter.h>
#include <usb_net.h>


struct task_struct *lx_user_new_usb_task(int (*func)(void*), void *args,
                                         char const *name)
{
	int pid = kernel_thread(func, args, name, CLONE_FS | CLONE_FILES);
	return find_task_by_pid_ns(pid, NULL);
}


void lx_user_init(void)
{
	lx_emul_usb_client_init();
	lx_emul_nic_init();
}


void lx_user_handle_io(void)
{
	lx_emul_usb_client_ticker();
	lx_emul_nic_handle_io();
}


#include <linux/rtnetlink.h>

/*
 * Called whenever the link state changes
 */

static bool force_uplink_destroy = false;

void rtmsg_ifinfo(int type, struct net_device * dev,
                  unsigned int change, gfp_t flags,
                  u32 portid, const struct nlmsghdr *nlh)
{
	lx_emul_nic_handle_io();

	if (force_uplink_destroy) {
		struct genode_uplink *uplink = (struct genode_uplink *)dev->ifalias;
		printk("force destroy uplink for net device %s\n", &dev->name[0]);
		genode_uplink_destroy(uplink);
		force_uplink_destroy = false;
	}
}


void lx_emul_usb_client_device_unregister_callback(struct usb_device *)
{
	force_uplink_destroy   = true;

	/* set mac as unconfigured by setting nothing */
	lx_emul_nic_set_mac_address(NULL, 0);
}


/*
 * Handle WDM device class for MBIM-modems
 */

struct usb_class_driver *wdm_driver;
struct file wdm_file;

enum { WDM_MINOR = 8 };

int usb_register_dev(struct usb_interface *intf, struct usb_class_driver *class_driver)
{
	if (strncmp(class_driver->name, "cdc-wdm", 7) == 0) {
		wdm_driver = class_driver;

		intf->usb_dev = &intf->dev;
		intf->minor   = WDM_MINOR;

		lx_wdm_create_root();
		return 0;
	}

	printk("%s:%d error: no device class for driver %s\n", __func__, __LINE__,
	       class_driver->name);

	return -1;
}


void usb_deregister_dev(struct usb_interface * intf,struct usb_class_driver * class_driver)
{
	lx_emul_trace(__func__);
}



int lx_wdm_read(void *args)
{
	ssize_t length;
	struct lx_wdm *wdm_data = (struct lx_wdm *)args;

	lx_emul_task_schedule(true);

	if (!wdm_driver) {
		printk("%s:%d error: no WDM class driver\n", __func__, __LINE__);
		return -1;
	}

	while (wdm_data->active) {
		length = wdm_driver->fops->read(&wdm_file, wdm_data->buffer, 0x1000, NULL);
		if (length > 0) {
			*wdm_data->data_avail = length;
			lx_wdm_signal_data_avail(wdm_data->handle);
		}
		lx_emul_task_schedule(true);
	}

	return 0;
}


int lx_wdm_write(void *args)
{
	ssize_t length;
	struct lx_wdm *wdm_data = (struct lx_wdm *)args;

	lx_emul_task_schedule(true);

	if (!wdm_driver) {
		printk("%s:%d error: no WDM class driver\n", __func__, __LINE__);
		return -1;
	}

	while (wdm_data->active) {
		length = wdm_driver->fops->write(&wdm_file, wdm_data->buffer,
		                                 *wdm_data->data_avail, NULL);
		if (length < 0) {
			printk("WDM write error: %ld", (long)length);
		}

		lx_wdm_schedule_read(wdm_data->handle);
		lx_emul_task_schedule(true);
	}
	return 0;
}


int lx_wdm_device(void *args)
{
	int err = -1;

	/* minor number for inode is 1 (see: ubs_register_dev above) */
	struct inode inode;
	inode.i_rdev = MKDEV(USB_DEVICE_MAJOR, WDM_MINOR);

	if (!wdm_driver) {
		printk("%s:%d error: no WDM class driver\n", __func__, __LINE__);
		return err;
	}

	if ((err = wdm_driver->fops->open(&inode, &wdm_file))) {
		printk("Could not open WDM device: %d", err);
		return err;
	}

	lx_emul_task_schedule(true);
	//XXX: close
	return 0;
}
