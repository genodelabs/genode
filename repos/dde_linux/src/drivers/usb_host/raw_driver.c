/*
 * \brief  Low level USB access driver
 * \author Sebastian Sumpf
 * \date   2014-11-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/usb.h>
#include "raw.h"

static int raw_probe(struct usb_device *udev)
{
	printk("RAW: vendor: %x product: %x dev %p\n", 
	       udev->descriptor.idVendor, udev->descriptor.idProduct, udev);

	return -ENODEV;
}

static void  raw_disconnect(struct usb_device *udev)
{
	printk("driver disconnect called\n");
}


struct usb_device_driver raw_driver =
{
	.name       = "raw",
	.probe      = raw_probe,
	.disconnect = raw_disconnect,
	.supports_autosuspend = 0,
};


static int raw_intf_probe(struct usb_interface *intf,
                          struct usb_device_id const *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);

	printk("RAW_INTF: vendor: %04x product: %04x\n", udev->descriptor.idVendor,
	       udev->descriptor.idProduct);

	return -ENODEV;
}

void raw_intf_disconnect(struct usb_interface *intf) { }

static const struct usb_device_id raw_intf_id_table[] = {
	{ .driver_info = 1 }
};


struct usb_driver raw_intf_driver =
{
	.name       = "rawintf",
	.probe      = raw_intf_probe,
	.disconnect = raw_intf_disconnect,
	.supports_autosuspend = 0,
};


struct notifier_block usb_nb =
{
	.notifier_call = raw_notify
};


static int raw_driver_init(void)
{
	int err;

	if ((err = usb_register_device_driver(&raw_driver, THIS_MODULE)))
		return err;

	printk("RAW: driver registered\n");

	if ((err = usb_register(&raw_intf_driver)))
		return err;

	printk("RAW: interface driver registered\n");

	usb_register_notify(&usb_nb);

	printk("RAW: notify function registered\n");

	return 0;
}

module_init(raw_driver_init);
