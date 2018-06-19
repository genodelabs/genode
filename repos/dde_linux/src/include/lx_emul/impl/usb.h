/*
 * \brief  Implementation of linux/usb.h
 * \author Stefan Kalkowski
 * \date   2018-08-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <lx_kit/malloc.h>
#include <lx_kit/usb.h>

int usb_control_msg(struct usb_device *dev, unsigned int pipe,
                    __u8 request, __u8 requesttype, __u16 value,
                    __u16 index, void *data, __u16 size, int timeout)
{
	usb_ctrlrequest *dr = (usb_ctrlrequest*)
		kmalloc(sizeof(usb_ctrlrequest), GFP_KERNEL);
	if (!dr) return -ENOMEM;

	dr->bRequestType = requesttype;
	dr->bRequest     = request;
	dr->wValue       = cpu_to_le16(value);
	dr->wIndex       = cpu_to_le16(index);
	dr->wLength      = cpu_to_le16(size);

	urb * u = (urb*) usb_alloc_urb(0, GFP_KERNEL);
	if (!u) return -ENOMEM;

	usb_fill_control_urb(u, dev, pipe, (unsigned char *)dr, data,
	                     size, nullptr, nullptr);

	Sync_ctrl_urb * scu = new (Lx::Malloc::mem())
		Sync_ctrl_urb(*(Usb::Connection*)(dev->bus->controller), *u);
	scu->send(timeout);
	int ret = u->actual_length;
	usb_free_urb(u);
	return ret;
}


struct urb *usb_alloc_urb(int iso_packets, gfp_t mem_flags)
{
	struct urb *urb = (struct urb*)
		kmalloc(sizeof(struct urb) +
	            iso_packets * sizeof(struct usb_iso_packet_descriptor),
	            GFP_KERNEL);
	if (!urb) return NULL;
	Genode::memset(urb, 0, sizeof(*urb));
	INIT_LIST_HEAD(&urb->anchor_list);
	return urb;
}


int usb_submit_urb(struct urb *urb, gfp_t mem_flags)
{
	Urb * u = new (Lx::Malloc::mem())
		Urb(*(Usb::Connection*)(urb->dev->bus->controller), *urb);
	u->send();
	return 0;
}


void usb_free_urb(struct urb *urb)
{
	kfree(urb);
}
