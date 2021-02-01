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
	if (!u) {
		kfree(dr);
		return -ENOMEM;
	}

	Sync_ctrl_urb * scu = (Sync_ctrl_urb *)kzalloc(sizeof(Sync_ctrl_urb), GFP_KERNEL);
	if (!scu) {
		usb_free_urb(u);
		kfree(dr);
		return -ENOMEM;
	}

	usb_fill_control_urb(u, dev, pipe, (unsigned char *)dr, data,
	                     size, nullptr, nullptr);

	Genode::construct_at<Sync_ctrl_urb>(scu, *(Usb::Connection*)(dev->bus->controller), *u);

	scu->send(timeout);
	kfree(scu);

	int ret;
	if (u->status >= 0)
		ret = u->actual_length;
	else
		ret = u->status;

	usb_free_urb(u);
	kfree(dr);
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
	Urb * u = (Urb *)kzalloc(sizeof(Urb), mem_flags);
	if (!u)
		return 1;

	Genode::construct_at<Urb>(u, *(Usb::Connection*)(urb->dev->bus->controller), *urb);

	/*
	 * Self-destruction of the 'Urb' object in its completion function
	 * would not work if the 'Usb' session gets closed before the
	 * completion function was called. So we store the pointer in the
	 * otherwise unused 'hcpriv' member and free it in a following
	 * 'usb_submit_urb()' or 'usb_free_urb()' call.
	 */

	if (urb->hcpriv) {
		Urb *prev_urb = (Urb*)urb->hcpriv;
		prev_urb->~Urb();
		kfree(urb->hcpriv);
	}

	urb->hcpriv = u;

	u->send();
	return 0;
}


void usb_free_urb(struct urb *urb)
{
	if (!urb)
		return;

	/* free 'Urb' object */
	if (urb->hcpriv) {
		Urb *u = (Urb*)urb->hcpriv;
		u->~Urb();
		kfree(urb->hcpriv);
	}

	kfree(urb);
}
