/*
 * \brief  Post kernel userland activity
 * \author Stefan Kalkowski
 * \date   2021-07-14
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include <lx_emul/task.h>
#include <lx_user/init.h>
#include <lx_user/io.h>

#include <usb.h>

struct usb_find_request {
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	struct usb_device  * ret;
};

static int check_usb_device(struct usb_device *usb_dev, void * data)
{
	struct usb_find_request * req = (struct usb_find_request *) data;
	if (usb_dev->devnum == req->dev && usb_dev->bus->busnum == req->bus)
		req->ret = usb_dev;
	return 0;
}

static struct usb_device * find_usb_device(genode_usb_bus_num_t bus,
                                           genode_usb_dev_num_t dev)
{
	struct usb_find_request req = { bus, dev, NULL };
	usb_for_each_dev(&req, check_usb_device);
	return req.ret;
}


static struct usb_interface * interface(genode_usb_bus_num_t bus,
                                        genode_usb_dev_num_t dev,
                                        unsigned index)
{
	struct usb_device * udev = find_usb_device(bus, dev);

	if (!udev)
		return NULL;

	if (!udev->actconfig)
		return NULL;

	if (index >= udev->actconfig->desc.bNumInterfaces)
		return NULL;

	return udev->actconfig->interface[index];
}


static unsigned config_descriptor(genode_usb_bus_num_t bus,
                                  genode_usb_dev_num_t dev,
                                  void * dev_desc, void *conf_desc)
{
	struct usb_device * udev = find_usb_device(bus, dev);
	if (!udev)
		return 0;

	memcpy(dev_desc, &udev->descriptor, sizeof(struct usb_device_descriptor));
	if (udev->actconfig)
		memcpy(conf_desc, &udev->actconfig->desc,
		       sizeof(struct usb_config_descriptor));
	else
		memset(conf_desc, 0, sizeof(struct usb_config_descriptor));

	return udev->speed;
}


static int alt_settings(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev,
                        unsigned index)
{
	struct usb_interface * iface = interface(bus, dev, index);
	return (iface) ? iface->num_altsetting : -1;
}


static int interface_descriptor(genode_usb_bus_num_t bus,
                                genode_usb_dev_num_t dev,
                                unsigned index, unsigned setting,
                                void * buf, unsigned long size, int * active)
{
	struct usb_interface * iface = interface(bus, dev, index);

	if (!iface)
		return -1;

	memcpy(buf, &iface->altsetting[setting].desc,
	       min(sizeof(struct usb_interface_descriptor), size));

	*active = &iface->altsetting[setting] == iface->cur_altsetting;
	return 0;
}


static int interface_extra(genode_usb_bus_num_t bus,
                           genode_usb_dev_num_t dev,
                           unsigned index, unsigned setting,
                           void * buf, unsigned long size)
{
	struct usb_interface * iface = interface(bus, dev, index);
	unsigned long len;

	if (!iface)
		return -1;

	len = min((unsigned long)iface->altsetting[setting].extralen, size);
	memcpy(buf, iface->altsetting[setting].extra, len);
	return len;
}


static int endpoint_descriptor(genode_usb_bus_num_t bus,
                               genode_usb_dev_num_t dev,
                               unsigned iface_num, unsigned setting,
                               unsigned endp, void * buf, unsigned long size)
{
	struct usb_device * udev = find_usb_device(bus, dev);
	struct usb_interface * iface;
	struct usb_host_endpoint * ep;

	if (!udev)
		return -1;
	iface = usb_ifnum_to_if(udev, iface_num);

	if (!iface)
		return -2;

	ep = &iface->altsetting[setting].endpoint[endp];
	if (!ep)
		return -3;

	memcpy(buf, &ep->desc,
	       min(sizeof(struct usb_endpoint_descriptor), size));

	return 0;
}


struct genode_usb_rpc_callbacks genode_usb_rpc_callbacks_obj = {
	.alloc_fn        = genode_usb_allocate_peer_buffer,
	.free_fn         = genode_usb_free_peer_buffer,
	.cfg_desc_fn     = config_descriptor,
	.alt_settings_fn = alt_settings,
	.iface_desc_fn   = interface_descriptor,
	.iface_extra_fn  = interface_extra,
	.endp_desc_fn    = endpoint_descriptor,
};


static genode_usb_request_ret_t
handle_ctrl_request(struct genode_usb_request_control * req,
                    void * buf, unsigned long size, void * data)
{
	struct usb_device * udev = (struct usb_device *) data;

	int pipe = (req->request_type & 0x80)
		? usb_rcvctrlpipe(udev, 0) : usb_sndctrlpipe(udev, 0);

	int err = usb_control_msg(udev, pipe, req->request, req->request_type,
	                          req->value, req->index, buf, size, req->timeout);

	if (err >= 0) {
		req->actual_size = err;
		return NO_ERROR;
	}

	req->actual_size = 0;

	switch (err) {
		case -ENOENT:    return INTERFACE_OR_ENDPOINT_ERROR;
		case -ENODEV:    return NO_DEVICE_ERROR;
		case -ESHUTDOWN: return NO_DEVICE_ERROR;
		case -EPROTO:    return PROTOCOL_ERROR;
		case -EILSEQ:    return PROTOCOL_ERROR;
		case -EPIPE:     return STALL_ERROR;
		case -ETIMEDOUT: return TIMEOUT_ERROR;
	}

	return UNKNOWN_ERROR;
}


static genode_usb_request_ret_t
handle_string_request(struct genode_usb_request_string * req,
                      void * buf, unsigned long size, void * data)
{
	struct usb_device * udev = (struct usb_device *) data;

	int length = usb_string(udev, req->index, buf, size);
	if (length < 0) {
		printk("Could not read string descriptor index: %u\n", req->index);
		req->length = 0;
	} else {
		/* returned length is in bytes (char) */
		req->length = length / 2;
		return NO_ERROR;
	}

	return UNKNOWN_ERROR;
}


static genode_usb_request_ret_t
handle_altsetting_request(unsigned iface, unsigned alt_setting, void * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	return (usb_set_interface(udev, iface, alt_setting)) ? NO_ERROR
	                                                     : UNKNOWN_ERROR;
}


static genode_usb_request_ret_t
handle_config_request(unsigned cfg_idx, void * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	return (usb_set_configuration(udev, cfg_idx)) ? UNKNOWN_ERROR : NO_ERROR;
}


static genode_usb_request_ret_t
handle_flush_request(unsigned char ep, void * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	struct usb_host_endpoint * endpoint =
		ep & USB_DIR_IN ? udev->ep_in[ep & 0xf]
		                : udev->ep_out[ep & 0xf];
	if (!endpoint)
		return INTERFACE_OR_ENDPOINT_ERROR;

	usb_hcd_flush_endpoint(udev, endpoint);
	return NO_ERROR;
}


static genode_usb_request_ret_t
handle_transfer_response(struct genode_usb_request_transfer * req,
                         void * data)
{
	struct urb * urb = (struct urb *) data;

	int i;

	if (urb->status == 0) {
		req->actual_size = urb->actual_length;

		if (usb_pipein(urb->pipe))
			for (i = 0; i < urb->number_of_packets; i++)
				req->actual_packet_size[i] = urb->iso_frame_desc[i].actual_length;

		return NO_ERROR;
	}

	switch (urb->status) {
		case -ESHUTDOWN: return NO_DEVICE_ERROR;
		case -EPROTO:    return PROTOCOL_ERROR;
		case -EILSEQ:    return PROTOCOL_ERROR;
		case -EPIPE:     return STALL_ERROR;
	};
	return UNKNOWN_ERROR;
}


static void async_complete(struct urb *urb)
{
	unsigned long handle = (unsigned long)urb->context;
	genode_usb_session_handle_t session =
		(genode_usb_session_handle_t) (handle >> 16);
	genode_usb_request_handle_t request =
		(genode_usb_request_handle_t) (handle & 0xffff);

	genode_usb_ack_request(session, request,
	                       handle_transfer_response, (void*)urb);
	usb_free_urb(urb);
	lx_user_handle_io();
}


static int fill_bulk_urb(struct usb_device                  * udev,
                         struct genode_usb_request_transfer * req,
                         unsigned long                        handle,
                         void                               * buf,
                         unsigned long                        size,
                         int                                  read,
                         struct urb                        ** urb)
{
	int pipe = (read)
		? usb_rcvbulkpipe(udev, req->ep) : usb_sndbulkpipe(udev, req->ep);

	*urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!*urb)
		return -ENOMEM;

	usb_fill_bulk_urb(*urb, udev, pipe, buf, size,
	                  async_complete, (void*)handle);
	return 0;
}


static int fill_irq_urb(struct usb_device                  * udev,
                        struct genode_usb_request_transfer * req,
                        unsigned long                        handle,
                        void                               * buf,
                        unsigned long                        size,
                        int                                  read,
                        struct urb                        ** urb)
{
	int polling_interval;
	int pipe = (read)
		? usb_rcvintpipe(udev, req->ep) : usb_sndintpipe(udev, req->ep);

	*urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!*urb)
		return -ENOMEM;

	if (req->polling_interval == -1) {

		struct usb_host_endpoint *ep = (req->ep & USB_DIR_IN) ?
			udev->ep_in[req->ep & 0xf] : udev->ep_out[req->ep & 0xf];

		if (!ep)
			return -ENOENT;

		polling_interval = ep->desc.bInterval;
	} else
		polling_interval = req->polling_interval;

	usb_fill_int_urb(*urb, udev, pipe, buf, size,
	                 async_complete, (void*)handle, polling_interval);
	return 0;
}


static int fill_isoc_urb(struct usb_device                  * udev,
                         struct genode_usb_request_transfer * req,
                         unsigned long                        handle,
                         void                               * buf,
                         unsigned long                        size,
                         int                                  read,
                         struct urb                        ** urb)
{
	int i;
	unsigned offset = 0;
	int pipe = (read)
		? usb_rcvisocpipe(udev, req->ep) : usb_sndisocpipe(udev, req->ep);
	struct usb_host_endpoint * ep =
		req->ep & USB_DIR_IN ? udev->ep_in[req->ep & 0xf]
		                     : udev->ep_out[req->ep & 0xf];

	*urb = usb_alloc_urb(req->number_of_packets, GFP_KERNEL);
	if (!*urb)
		return -ENOMEM;

	(*urb)->dev                    = udev;
	(*urb)->pipe                   = pipe;
	(*urb)->start_frame            = -1;
	(*urb)->stream_id              = 0;
	(*urb)->transfer_buffer        = buf;
	(*urb)->transfer_buffer_length = size;
	(*urb)->number_of_packets      = req->number_of_packets;
	(*urb)->interval               = 1 << min(15, ep->desc.bInterval - 1);
	(*urb)->context                = (void *)handle;
	(*urb)->transfer_flags         = URB_ISO_ASAP | (read ? URB_DIR_IN : URB_DIR_OUT);
	(*urb)->complete               = async_complete;

	for (i = 0; i < req->number_of_packets; i++) {
		(*urb)->iso_frame_desc[i].offset = offset;
		(*urb)->iso_frame_desc[i].length = req->packet_size[i];
		offset += req->packet_size[i];
	}

	return 0;
}


static genode_usb_request_ret_t
handle_transfer_request(struct genode_usb_request_transfer * req,
                        genode_usb_transfer_type_t           type,
                        genode_usb_session_handle_t          session_handle,
                        genode_usb_request_handle_t          request_handle,
                        void * buf, unsigned long size, void * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	int err  = 0;
	int read = (req->ep & 0x80);
	unsigned long handle = session_handle << 16 | request_handle;
	struct urb * urb;

	switch (type) {
	case BULK:
		err = fill_bulk_urb(udev, req, handle, buf, size, read, &urb);
		break;
	case IRQ:
		err = fill_irq_urb(udev, req, handle, buf, size, read, &urb);
		break;
	case ISOC:
		err = fill_isoc_urb(udev, req, handle, buf, size, read, &urb);
		break;
	default:
		printk("Unknown USB transfer request!\n");
		return UNKNOWN_ERROR;
	};

	if (!err) {
		err = usb_submit_urb(urb, GFP_KERNEL);

		if (!err)
			return NO_ERROR;

		usb_free_urb(urb);
	}

	switch (err) {
		case -ENOENT:    return INTERFACE_OR_ENDPOINT_ERROR;
		case -ENODEV:    return NO_DEVICE_ERROR;
		case -ESHUTDOWN: return NO_DEVICE_ERROR;
		case -ENOSPC:    return STALL_ERROR;
		case -ENOMEM:    return MEMORY_ERROR;
	}
	return UNKNOWN_ERROR;
}


static struct genode_usb_request_callbacks request_callbacks = {
	.control_fn    = handle_ctrl_request,
	.transfer_fn   = handle_transfer_request,
	.string_fn     = handle_string_request,
	.altsetting_fn = handle_altsetting_request,
	.config_fn     = handle_config_request,
	.flush_fn      = handle_flush_request,
};


static int poll_usb_device(struct usb_device *udev, void * data)
{
	genode_usb_session_handle_t session =
		genode_usb_session_by_bus_dev(udev->bus->busnum, udev->devnum);
	int * work_done = (int *) data;

	if (!session)
		return 0;

	for (;;) {
		if (!genode_usb_request_by_session(session, &request_callbacks,
		                                   (void*)udev))
			break;
		*work_done = true;
	}

	return 0;
}


static int usb_poll_sessions(void * data)
{
	for (;;) {
		int work_done = false;
		usb_for_each_dev(&work_done, poll_usb_device);
		if (work_done)
			continue;
		lx_emul_task_schedule(true);
	}

	return 0;
}


static struct task_struct * lx_user_task = NULL;


void lx_user_handle_io(void)
{
	if (lx_user_task)
		lx_emul_task_unblock(lx_user_task);
}


void lx_user_init(void)
{
	int pid = kernel_thread(usb_poll_sessions, NULL, CLONE_FS | CLONE_FILES);
	lx_user_task = find_task_by_pid_ns(pid, NULL);;
}


static int raw_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	struct usb_device *udev = (struct usb_device*) data;

	switch (action) {

		case USB_DEVICE_ADD:
		{
			/**
			 * Register pseudo device class of USB device
			 *
			 * The registered value expresses the type of USB device.
			 * If the device has at least one HID interface, the value
			 * is USB_CLASS_HID. Otherwise, the class of the first interface
			 * is interpreted as device type.
			 *
			 * Note this classification of USB devices is meant as an interim
			 * solution only to assist the implementation of access-control
			 * policies.
			 */
			unsigned long class;
			unsigned i;

			for (i = 0; i < udev->actconfig->desc.bNumInterfaces; i++) {
				struct usb_interface * iface = udev->actconfig->interface[i];
				if (!iface || !iface->cur_altsetting) continue;
				if (i == 0 ||
				    iface->cur_altsetting->desc.bInterfaceClass ==
				    USB_CLASS_HID)
					class = iface->cur_altsetting->desc.bInterfaceClass;
			}

			genode_usb_announce_device(udev->descriptor.idVendor,
			                           udev->descriptor.idProduct,
			                           class,
			                           udev->bus->busnum,
			                           udev->devnum);
			break;
		}

		case USB_DEVICE_REMOVE:
		{
			genode_usb_discontinue_device(udev->bus->busnum, udev->devnum);
			break;
		}

		case USB_BUS_ADD:
			break;

		case USB_BUS_REMOVE:
			break;
    } 

    return NOTIFY_OK;
}


struct notifier_block usb_nb =
{
	.notifier_call = raw_notify
};


static int usbnet_init(void)
{
	usb_register_notify(&usb_nb);
	return 0;
}

/**
 * Let's hook into the usbnet initcall, so we do not need to register
 * an additional one
 */
module_init(usbnet_init);
