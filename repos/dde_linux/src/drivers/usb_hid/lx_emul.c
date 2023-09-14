/*
 * \brief  Implementation of driver specific Linux functions
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

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/usb.h>

#include <usb_hid.h>

const struct attribute_group input_poller_attribute_group;
pteval_t __default_kernel_pte_mask __read_mostly = ~0;

struct usb_driver usbfs_driver = {
	.name = "usbfs"
};
const struct attribute_group *usb_device_groups[] = { };
