/*
 * \brief  urb.c functions using genode_c_api/usb_client.h
 * \author Sebastian Sumpf
 * \date   2023-06-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <genode_c_api/usb_client.h>

#include "urb_helper.h"

#define to_urb(d) container_of(d, struct urb, kref)


static DECLARE_WAIT_QUEUE_HEAD(lx_emul_urb_wait);

int wait_for_free_urb(unsigned int timeout_jiffies)
{
	int ret = 0;

	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(&lx_emul_urb_wait, &wait);

	ret = schedule_timeout(timeout_jiffies);

	remove_wait_queue(&lx_emul_urb_wait, &wait);

	return ret;
}


struct urb *usb_alloc_urb(int iso_packets, gfp_t mem_flags)
{
	struct urb *urb = (struct urb*)
		kmalloc(sizeof(struct urb) +
		        iso_packets * sizeof(struct usb_iso_packet_descriptor),
		        GFP_KERNEL);

	if (!urb) return NULL;
	memset(urb, 0, sizeof(*urb));
	kref_init(&urb->kref);
	INIT_LIST_HEAD(&urb->urb_list);
	INIT_LIST_HEAD(&urb->anchor_list);

	return urb;
}


static void free_packet(struct genode_usb_client_request_packet *packet)
{
	kfree(packet->request.req);
	kfree(packet);
}


static void urb_submit_complete(struct genode_usb_client_request_packet *packet)
{
	struct urb *urb = (struct urb *)packet->opaque_data;
	genode_usb_client_handle_t handle = (genode_usb_client_handle_t)urb->hcpriv;

	urb->status = packet->error ? packet_errno(packet->error) : 0;

	if (packet->error == 0 &&
	    packet->actual_length && urb->transfer_buffer &&
	    urb->transfer_buffer_length >= packet->actual_length)
		memcpy(urb->transfer_buffer, packet->buffer.addr, packet->actual_length);

	urb->actual_length = packet->actual_length;

	genode_usb_client_request_finish(handle, packet);

	free_packet(packet);

	if (urb->complete) urb->complete(urb);
};


int usb_submit_urb(struct urb *urb, gfp_t mem_flags)
{
	genode_usb_client_handle_t handle;
	struct genode_usb_client_request_packet *packet;
	struct genode_usb_request_transfer *transfer;
	struct genode_usb_request_control  *control;
	int ret = 0;
	unsigned timeout_jiffies = msecs_to_jiffies(10000u);

	if (!urb->dev->bus)
		return -ENODEV;

	handle = (genode_usb_client_handle_t)urb->dev->bus->controller;

	packet = (struct genode_usb_client_request_packet *)
		kzalloc(sizeof(*packet), GFP_KERNEL);

	if (!packet) return -ENOMEM;

	if (usb_pipetype(urb->pipe) == PIPE_CONTROL) {
		control = (struct genode_usb_request_control *)
			kzalloc(sizeof(*control), GFP_KERNEL);

		if (!control) {
			ret = -ENOMEM;
			goto transfer;
		}
	}
	else {
		transfer = (struct genode_usb_request_transfer *)
			kzalloc(sizeof(*transfer), GFP_KERNEL);

		if (!transfer) {
			ret = -ENOMEM;
			goto transfer;
		}
	}

	switch(usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		{
			struct usb_ctrlrequest * ctrl = (struct usb_ctrlrequest *)
				urb->setup_packet;
			packet->request.type  = CTRL;
			control->request      = ctrl->bRequest;
			control->request_type = ctrl->bRequestType;
			control->value        = ctrl->wValue;
			control->index        = ctrl->wIndex;
			packet->request.req   = control;
			break;
		}
	case PIPE_INTERRUPT:
		{
			packet->request.type       = IRQ;
			transfer->polling_interval = urb->interval;
			transfer->ep               = usb_pipeendpoint(urb->pipe)
			                             | (usb_pipein(urb->pipe) ?  USB_DIR_IN : 0);
			packet->request.req        = transfer;
			break;
		}
	case PIPE_BULK:
		{
			packet->request.type = BULK;
			transfer->ep         = usb_pipeendpoint(urb->pipe)
			                       | (usb_pipein(urb->pipe) ?  USB_DIR_IN : 0);
			packet->request.req  = transfer;
			break;
		}
	default:
		printk("unknown URB requested: %d\n", usb_pipetype(urb->pipe));
	}

	packet->buffer.size       = urb->transfer_buffer_length;
	packet->complete_callback = urb_submit_complete;
	packet->opaque_data       = urb;
	packet->free_callback     = free_packet;

	for (;;) {

		if (genode_usb_client_request(handle, packet)) break;

		timeout_jiffies = wait_for_free_urb(timeout_jiffies);
		if (!timeout_jiffies) {
			ret = -ETIMEDOUT;
			goto err_request;
		}
	}

	if (usb_pipeout(urb->pipe))
		memcpy(packet->buffer.addr, urb->transfer_buffer, urb->transfer_buffer_length);

	urb->hcpriv = (void *)handle;

	genode_usb_client_request_submit(handle, packet);

	return ret;

err_request:
	if (transfer) kfree(transfer);
	if (control)  kfree(control);
transfer:
	kfree(packet);

	return ret;
}


struct urb *usb_get_urb(struct urb *urb)
{
	if (urb)
		kref_get(&urb->kref);
	return urb;
}


static void urb_destroy(struct kref *kref)
{
	struct urb *urb = to_urb(kref);

	kfree(urb);
	wake_up(&lx_emul_urb_wait);
}


/* usb_put_urb is defined as usb_free_urb, therefore we need reference counting */
void usb_free_urb(struct urb *urb)
{
	if (urb)
		kref_put(&urb->kref, urb_destroy);
}
