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
#include <legacy/lx_kit/usb.h>

static DECLARE_WAIT_QUEUE_HEAD(lx_emul_urb_wait);

static int wait_for_free_urb(unsigned int timeout_jiffies)
{
	int ret = 0;

	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(&lx_emul_urb_wait, &wait);

	ret = schedule_timeout(timeout_jiffies);

	remove_wait_queue(&lx_emul_urb_wait, &wait);

	return ret;
}


int usb_control_msg(struct usb_device *dev, unsigned int pipe,
                    __u8 request, __u8 requesttype, __u16 value,
                    __u16 index, void *data, __u16 size, int timeout)
{
	Usb::Connection *usb;
	Sync_ctrl_urb *scu;
	urb *u;
	usb_ctrlrequest *dr;
	unsigned int timeout_jiffies;
	int ret;

	dr = (usb_ctrlrequest*)kmalloc(sizeof(usb_ctrlrequest), GFP_KERNEL);
	if (!dr) return -ENOMEM;

	dr->bRequestType = requesttype;
	dr->bRequest     = request;
	dr->wValue       = cpu_to_le16(value);
	dr->wIndex       = cpu_to_le16(index);
	dr->wLength      = cpu_to_le16(size);

	u = (urb*) usb_alloc_urb(0, GFP_KERNEL);
	if (!u) {
		ret = -ENOMEM;
		goto err_urb;
	}

	scu = (Sync_ctrl_urb *)kzalloc(sizeof(Sync_ctrl_urb), GFP_KERNEL);
	if (!scu) {
		ret = -ENOMEM;
		goto err_scu;
	}

	usb_fill_control_urb(u, dev, pipe, (unsigned char *)dr, data,
	                     size, nullptr, nullptr);

	if (!dev->bus || !dev->bus->controller) {
		ret = -ENODEV;
		goto err_fill;
	}

	/*
	 * If this function is called with a timeout of 0 to wait forever,
	 * we wait in pieces of 10s each as 'schedule_timeout' might trigger
	 * immediately otherwise. The intend to wait forever is reflected
	 * back nonetheless when sending the urb.
	 */
	timeout_jiffies = timeout ? msecs_to_jiffies(timeout)
	                          : msecs_to_jiffies(10000u);

	usb = (Usb::Connection*)(dev->bus->controller);
	for (;;) {
		if (usb->source()->ready_to_submit(1))
			try {
				Genode::construct_at<Sync_ctrl_urb>(scu, *usb, *u);
				break;
			} catch (...) { }

		timeout_jiffies = wait_for_free_urb(timeout_jiffies);
		if (!timeout_jiffies && timeout) {
			ret = -ETIMEDOUT;
			goto err_fill;
		}
	}

	scu->send(timeout ? jiffies_to_msecs(timeout_jiffies) : 0);

	ret = u->status >= 0 ? u->actual_length : u->status;

err_fill:
	kfree(scu);
err_scu:
	usb_free_urb(u);
err_urb:
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
	if (!urb->dev->bus || !urb->dev->bus->controller)
		return -ENODEV;

	Urb * u = (Urb *)kzalloc(sizeof(Urb), mem_flags);
	if (!u)
		return 1;

	Usb::Connection &usb = *(Usb::Connection*)(urb->dev->bus->controller);
	for (;;) {
		if (usb.source()->ready_to_submit(1))
			try {
				Genode::construct_at<Urb>(u, usb, *urb);
				break;
			} catch (...) { }

		(void)wait_for_free_urb(msecs_to_jiffies(10000u));
	}

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

		/* URB is not fred through packet stream */
		if (u->completed() == false) return;

		u->~Urb();
		kfree(urb->hcpriv);
	}

	kfree(urb);

	wake_up(&lx_emul_urb_wait);
}
