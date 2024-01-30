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

struct device_type usb_if_device_type = {
	.name = "usb_interface"
};

struct usb_driver usbfs_driver = {
	.name = "usbfs"
};
const struct attribute_group *usb_device_groups[] = { };


/*
 * Taken and ajdusted from linux/drivers/usb/core/message.c If this is required
 * by more drivers in the future, we will move it to
 * dde_linux/src/lib/virt/shadow/drivers/usb/core/message.c
 */
int usb_clear_halt(struct usb_device *dev, int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe);

	if (usb_pipein(pipe))
		endp |= USB_DIR_IN;

	/* we don't care if it wasn't halted first. in fact some devices
	 * (like some ibmcam model 1 units) seem to expect hosts to make
	 * this request for iso endpoints, which can't halt!
	 */
	result = usb_control_msg(dev, usb_sndctrlpipe(dev, endp),
	                         USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT,
	                         USB_ENDPOINT_HALT, endp, NULL,  0,
	                         USB_CTRL_SET_TIMEOUT);
	return result;
}
