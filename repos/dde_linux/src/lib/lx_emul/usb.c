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

#define KBUILD_MODNAME "genode_usb_driver"

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <uapi/linux/usbdevice_fs.h>

#include <lx_emul/shared_dma_buffer.h>
#include <lx_emul/task.h>
#include <lx_emul/usb.h>
#include <lx_user/init.h>
#include <lx_user/io.h>

#include <genode_c_api/usb.h>

struct usb_interface;

static int usb_drv_probe(struct usb_interface *interface,
                         const struct usb_device_id *id) {
	return -ENODEV; }


static void usb_drv_disconnect(struct usb_interface *iface) { }


static struct usb_driver usb_drv = {
	.name       = "genode",
	.probe      = usb_drv_probe,
	.disconnect = usb_drv_disconnect,
	.supports_autosuspend = 0,
};


static genode_usb_request_ret_t handle_return_code(int err)
{
	/*
	 * USB error codes are documented in kernel sources
	 *
	 * Documentation/driver-api/usb/error-codes.rst
	 */
	switch (err) {
	case 0:          return OK;
	case -ENOENT:    return NO_DEVICE;
	case -ENODEV:    return NO_DEVICE;
	case -ESHUTDOWN: return NO_DEVICE;
	case -EILSEQ:    return NO_DEVICE; /* xHCI ret val when HID vanishs */
	case -EPROTO:    return NO_DEVICE;
	case -ETIMEDOUT: return TIMEOUT;
	case -ENOSPC:    return HALT;
	case -EPIPE:     return HALT;
	case -ENOMEM:    return INVALID;
	case -EINVAL:    return INVALID;
	default:         return INVALID;
	};
};


struct usb_per_dev_data
{
	struct usb_device  * dev;
	struct task_struct * task;
	struct usb_anchor    submitted;
	bool                 kill_task;
};


static int poll_usb_device(void * args);
static void open_usb_dev(struct usb_device * udev)
{
	struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);

	if (!data) {
		int pid;
		data = kmalloc(sizeof(struct usb_per_dev_data), GFP_KERNEL);
		data->dev = udev;
		data->kill_task = false;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
		pid = kernel_thread(poll_usb_device, data, "poll_device",
		                    CLONE_FS | CLONE_FILES);
#else
		pid = kernel_thread(poll_usb_device, data, CLONE_FS | CLONE_FILES);
#endif

		data->task = find_task_by_pid_ns(pid, NULL);
		init_usb_anchor(&data->submitted);
		dev_set_drvdata(&udev->dev, data);
	}
}


static void release_device(struct usb_per_dev_data * data)
{
	unsigned int ifnum;

	usb_kill_anchored_urbs(&data->submitted);

	if (!data->dev)
		return;

	/**
	 * If the device gets released although it is still available,
	 * reset the device to be sane for new sessions aquiring it.
	 * Moreover, release each interface as part of reset, therefore
	 * they have to be claimed before.
	 */
	for (ifnum = 0; ifnum < 8; ifnum++) {
		struct usb_interface * iface = usb_ifnum_to_if(data->dev, ifnum);
		if (iface) {
			usb_driver_claim_interface(&usb_drv, iface, NULL);
			usb_driver_release_interface(&usb_drv, iface);
		}
	}
	usb_reset_device(data->dev);
}


static void
handle_control_request(genode_usb_request_handle_t handle,
                       unsigned char               ctrl_request,
                       unsigned char               ctrl_request_type,
                       unsigned short              ctrl_value,
                       unsigned short              ctrl_index,
                       unsigned long               ctrl_timeout,
                       genode_buffer_t             payload,
                       void                       *opaque_callback_data)
{
	struct usb_device *udev = (struct usb_device *) opaque_callback_data;
	int ret = 0;
	u32 size;
	bool send_msg = true;

	/* check for set alternate interface request */
	if (ctrl_request == USB_REQ_SET_INTERFACE &&
	    (ctrl_request_type & 0x7f) == (USB_TYPE_STANDARD|USB_RECIP_INTERFACE)) {
		struct usb_interface *iface = usb_ifnum_to_if(udev, ctrl_index);
		struct usb_host_interface *alt =
			iface ? usb_altnum_to_altsetting(iface, ctrl_value) : NULL;

		if (iface && iface->cur_altsetting != alt)
			ret = usb_set_interface(udev, ctrl_index, ctrl_value);

		send_msg = false;
	}

	/* check for set device configuration request */
	if (ctrl_request == USB_REQ_SET_CONFIGURATION &&
	    ctrl_request_type == USB_RECIP_DEVICE) {
		if (!(udev->actconfig &&
		      udev->actconfig->desc.bConfigurationValue == ctrl_value))
			ret = usb_set_configuration(udev, ctrl_value);
		send_msg = false;
	}

	/* otherwise send control message */
	if (send_msg) {
		int pipe = (ctrl_request_type & 0x80)
			? usb_rcvctrlpipe(udev, 0) : usb_sndctrlpipe(udev, 0);

			usb_unlock_device(udev);
			ret = usb_control_msg(udev, pipe, ctrl_request, ctrl_request_type,
			                      ctrl_value, ctrl_index, payload.addr,
			                      payload.size, ctrl_timeout);
			usb_lock_device(udev);
	}

	size = ret < 0 ? 0 : ret;
	genode_usb_ack_request(handle, handle_return_code(ret < 0 ? ret : 0),
	                       &size);
}


static void anchor_and_submit_urb(genode_usb_request_handle_t handle,
                                  struct urb                 *urb,
                                  struct usb_anchor          *anchor)
{
	int ret;
	usb_anchor_urb(urb, anchor);
	ret = usb_submit_urb(urb, GFP_KERNEL);

	if (!ret)
		return;

	usb_unanchor_urb(urb);
	usb_free_urb(urb);
	genode_usb_ack_request(handle, handle_return_code(ret), NULL);
}


static void async_complete(struct urb *urb)
{
	/* Linux kernel's devio layer limits ISOC packets to 128 */
	u32 sizes[128 + 1];

	genode_usb_request_handle_t handle =
		(genode_usb_request_handle_t) urb->context;
	sizes[0] = urb->actual_length;
	if (urb->status >= 0 && urb->number_of_packets > 0) {
		unsigned int i;
		for (i = 0; i < urb->number_of_packets; i++)
			sizes[i+1] = urb->iso_frame_desc[i].actual_length;
	}

	genode_usb_ack_request(handle, handle_return_code(urb->status), sizes);

	/* unblock device's task, it may process further URBs now */
	if (urb->dev) {
		struct usb_per_dev_data  *data = dev_get_drvdata(&urb->dev->dev);
		lx_emul_task_unblock(data->task);
	}

	usb_free_urb(urb);
}




static void
handle_irq_request(genode_usb_request_handle_t handle,
                   unsigned char               ep_addr,
                   genode_buffer_t             payload,
                   void                       *opaque_callback_data)
{
	struct usb_device        *udev = (struct usb_device *) opaque_callback_data;
	struct usb_per_dev_data  *data = dev_get_drvdata(&udev->dev);
	int pipe = (ep_addr & USB_DIR_IN) ? usb_rcvintpipe(udev, ep_addr & 0x7f)
	                                  : usb_sndintpipe(udev, ep_addr & 0x7f);
	struct usb_host_endpoint *ep = usb_pipe_endpoint(udev, pipe);
	struct urb *urb;

	if ((payload.size && !payload.addr) ||
	    !ep || !usb_endpoint_maxp(&ep->desc)) {
		int ret = ep ? -EINVAL : -ENODEV;
		genode_usb_ack_request(handle, handle_return_code(ret), NULL);
		return;
	}

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		genode_usb_ack_request(handle, handle_return_code(-ENOMEM), NULL);
		return;
	}

	usb_fill_int_urb(urb, udev, pipe, payload.addr, payload.size,
	                 async_complete, handle, ep->desc.bInterval);
	anchor_and_submit_urb(handle, urb, &data->submitted);
}


static void
handle_bulk_request(genode_usb_request_handle_t handle,
                   unsigned char                ep_addr,
                   genode_buffer_t              payload,
                   void                        *opaque_callback_data)
{
	struct usb_device       * udev = (struct usb_device *) opaque_callback_data;
	struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);
	struct urb *urb;

	int pipe = (ep_addr & USB_DIR_IN) ? usb_rcvbulkpipe(udev, ep_addr & 0x7f)
	                                  : usb_sndbulkpipe(udev, ep_addr & 0x7f);

	if (!payload.addr || payload.size >= (INT_MAX - sizeof(struct urb))) {
		genode_usb_ack_request(handle, handle_return_code(-EINVAL), NULL);
		return;
	}

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		genode_usb_ack_request(handle, handle_return_code(-ENOMEM), NULL);
		return;
	}

	usb_fill_bulk_urb(urb, udev, pipe, payload.addr, payload.size,
	                  async_complete, handle);
	anchor_and_submit_urb(handle, urb, &data->submitted);
}


static void
handle_isoc_request(genode_usb_request_handle_t        handle,
                    unsigned char                      ep_addr,
                    u32                                number_of_packets,
                    struct genode_usb_isoc_descriptor *packets,
                    genode_buffer_t                    payload,
                    void                              *opaque_callback_data)
{
	struct usb_device        *udev = (struct usb_device *) opaque_callback_data;
	struct usb_per_dev_data  *data = dev_get_drvdata(&udev->dev);
	int pipe = (ep_addr & USB_DIR_IN) ? usb_rcvisocpipe(udev, ep_addr & 0x7f)
	                                  : usb_sndisocpipe(udev, ep_addr & 0x7f);
	struct usb_host_endpoint *ep = usb_pipe_endpoint(udev, pipe);
	struct urb *urb;
	unsigned int i;
	unsigned offset = 0;

	if (!payload.addr           ||
	    number_of_packets > 128 ||
	    number_of_packets < 1   ||
	    !ep) {
		int ret = ep ? -EINVAL : -EINVAL;
		genode_usb_ack_request(handle, handle_return_code(ret), NULL);
		return;
	}

	urb = usb_alloc_urb(number_of_packets, GFP_KERNEL);
	if (!urb) {
		genode_usb_ack_request(handle, handle_return_code(-ENOMEM), NULL);
		return;
	}

	urb->dev                    = udev;
	urb->pipe                   = pipe;
	urb->start_frame            = -1;
	urb->stream_id              = 0;
	urb->transfer_buffer        = payload.addr;
	urb->transfer_buffer_length = payload.size;
	urb->number_of_packets      = number_of_packets;
	urb->interval               = 1 << min(15, ep->desc.bInterval - 1);
	urb->context                = handle;
	urb->transfer_flags         = URB_ISO_ASAP | (ep_addr & USB_ENDPOINT_DIR_MASK);
	urb->complete               = async_complete;

	for (i = 0; i < number_of_packets; i++) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = packets[i].size;
		offset += packets[i].size;
	}
	anchor_and_submit_urb(handle, urb, &data->submitted);
}


static void
handle_flush_request(unsigned char               ep_addr,
                     genode_usb_request_handle_t handle,
                     void                       *data)
{
	struct usb_device * udev = (struct usb_device *) data;
	u32 size = 0;
	int ret = udev ? 0 : -ENODEV;

	if (udev) {
		struct usb_host_endpoint * endpoint =
			ep_addr & USB_DIR_IN ? udev->ep_in[ep_addr  & 0xf]
			                     : udev->ep_out[ep_addr & 0xf];
		if (endpoint) usb_hcd_flush_endpoint(udev, endpoint);
	}

	genode_usb_ack_request(handle, handle_return_code(ret), &size);
}


static struct genode_usb_request_callbacks request_callbacks = {
	.ctrl_fn  = handle_control_request,
	.irq_fn   = handle_irq_request,
	.bulk_fn  = handle_bulk_request,
	.isoc_fn  = handle_isoc_request,
	.flush_fn = handle_flush_request,
};


static inline void exit_usb_task(struct usb_per_dev_data * data)
{
	struct usb_device * udev = (struct usb_device *) data->dev;
	release_device(data);
	if (udev) dev_set_drvdata(&udev->dev, NULL);
	kfree(data);
}


static inline bool check_for_urbs(struct usb_device * udev)
{
	return genode_usb_request_by_bus_dev(udev->bus->busnum, udev->devnum,
	                                     &request_callbacks, (void*)udev);
}


static int poll_usb_device(void * args)
{
	struct usb_per_dev_data * data = (struct usb_per_dev_data*)args;
	genode_usb_bus_num_t      bus  = data->dev->bus->busnum;
	genode_usb_dev_num_t      dev  = data->dev->devnum;

	for (;;) {
		if (data->dev) usb_lock_device(data->dev);
		while (data->dev && check_for_urbs(data->dev)) ;
		if (data->dev) usb_unlock_device(data->dev);

		/* check if device got removed */
		if (!data->dev)
			genode_usb_discontinue_device(bus, dev);

		if (data->kill_task) {
			exit_usb_task(data);
			do_exit(0);
		}
		lx_emul_task_schedule(true);
	}

	return 0;
}


static int wake_up_udev_task(struct usb_device *udev, void * args)
{
	struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);

	bool acquired = genode_usb_device_acquired(udev->bus->busnum,
	                                           udev->devnum);
	if (!acquired && !data)
		return 0;

	if (!data) {
		open_usb_dev(udev);
		data = dev_get_drvdata(&udev->dev);
	}

	if (!acquired && data)
		data->kill_task = true;

	lx_emul_task_unblock(data->task);
	return 0;
}


static int usb_poll_empty_sessions(void * data)
{
	for (;;) {
		usb_for_each_dev(NULL, wake_up_udev_task);
		lx_emul_task_schedule(false);
		genode_usb_handle_disconnected_sessions();
		lx_emul_task_schedule(true);
	}

	return 0;
}


static struct task_struct * lx_user_task = NULL;


void lx_user_handle_io(void)
{
	if (lx_user_task) lx_emul_task_unblock(lx_user_task);
}


void lx_user_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
	int pid = kernel_thread(usb_poll_empty_sessions, NULL,
	                        "usb_poll", CLONE_FS | CLONE_FILES);
#else
	int pid = kernel_thread(usb_poll_empty_sessions, NULL,
	                        CLONE_FS | CLONE_FILES);
#endif
	lx_user_task = find_task_by_pid_ns(pid, NULL);
}


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


static int device_released(void *d)
{
	struct usb_device *udev = (struct usb_device*)d;
	struct usb_per_dev_data * data = udev ? dev_get_drvdata(&udev->dev) : NULL;
	return !data;
}


void lx_emul_usb_release_device(genode_usb_bus_num_t bus,
                                genode_usb_dev_num_t dev)
{
	struct usb_device * udev = find_usb_device(bus, dev);
	struct usb_per_dev_data * data = udev ? dev_get_drvdata(&udev->dev) : NULL;
	bool acquired = genode_usb_device_acquired(bus, dev);

	if (acquired || !data)
		return;

	data->kill_task = true;
	lx_emul_task_unblock(data->task);
	lx_emul_execute_kernel_until(&device_released, udev);
}


static void add_endpoint_callback(struct genode_usb_interface * iface,
                                  unsigned idx, void * data)
{
	struct usb_host_interface *uiface = (struct usb_host_interface*) data;
	struct genode_usb_endpoint_descriptor *desc =
		(struct genode_usb_endpoint_descriptor*) &uiface->endpoint[idx].desc;
	genode_usb_device_add_endpoint(iface, *desc);
}


static void interface_string(genode_buffer_t string, void * data)
{
	struct usb_host_interface *uiface = (struct usb_host_interface*) data;
	if (uiface->string)
		strlcpy(string.addr, uiface->string, string.size);
	else
		*(char *)string.addr = 0;
}


static void add_interface_callback(struct genode_usb_configuration * cfg,
                                   unsigned idx, void * data)
{
	struct usb_host_config     *ucfg        = (struct usb_host_config*) data;
	struct usb_interface_cache *iface_cache = ucfg->intf_cache[idx];
	struct usb_interface       *iface       = ucfg->interface[idx];
	unsigned i;

	for (i = 0; i < iface_cache->num_altsetting; i++) {
		struct genode_usb_interface_descriptor *desc =
			(struct genode_usb_interface_descriptor*)
				&iface_cache->altsetting[i].desc;
		bool set = iface ? &iface->altsetting[i] == iface->cur_altsetting
		                 : false;
		genode_usb_device_add_interface(cfg, interface_string, *desc,
		                                add_endpoint_callback,
		                                &iface_cache->altsetting[i], set);
	}
}


static void add_configuration_callback(struct genode_usb_device * dev,
                                       unsigned idx, void * data)
{
	struct usb_device *udev = (struct usb_device*) data;
	struct genode_usb_config_descriptor *desc =
		(struct genode_usb_config_descriptor*) &udev->config[idx].desc;
	genode_usb_device_add_configuration(dev, *desc, add_interface_callback,
	                                    &udev->config[idx],
	                                    &udev->config[idx] == udev->actconfig);
}


static void manufacturer_string(genode_buffer_t string, void * data)
{
	struct usb_device *udev = (struct usb_device*) data;
	if (udev->manufacturer)
		strlcpy(string.addr, udev->manufacturer, string.size);
	else
		*(char *)string.addr = 0;
}


static void product_string(genode_buffer_t string, void * data)
{
	struct usb_device *udev = (struct usb_device*) data;
	if (udev->product)
		strlcpy(string.addr, udev->product, string.size);
	else
		*(char *)string.addr = 0;
}


static int raw_notify(struct notifier_block *nb, unsigned long action,
                      void *data)
{
	struct usb_device *udev = (struct usb_device*) data;

	switch (action) {

		case USB_DEVICE_ADD:
		{
			struct genode_usb_device_descriptor *desc =
				(struct genode_usb_device_descriptor*) &udev->descriptor;
			genode_usb_speed_t speed;
			switch (udev->speed) {
			case USB_SPEED_LOW:      speed = GENODE_USB_SPEED_LOW; break;
			case USB_SPEED_UNKNOWN:
			case USB_SPEED_FULL:     speed = GENODE_USB_SPEED_FULL; break;
			case USB_SPEED_HIGH:
			case USB_SPEED_WIRELESS: speed = GENODE_USB_SPEED_HIGH; break;
			case USB_SPEED_SUPER:    speed = GENODE_USB_SPEED_SUPER; break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,13,0)
			case USB_SPEED_SUPER_PLUS:
				if (udev->ssp_rate == USB_SSP_GEN_2x2)
					speed = GENODE_USB_SPEED_SUPER_PLUS_2X2;
				else
					speed = GENODE_USB_SPEED_SUPER_PLUS;
				break;
#endif
			default: speed = GENODE_USB_SPEED_FULL;
			}
			genode_usb_announce_device(udev->bus->busnum, udev->devnum, speed,
			                           manufacturer_string, product_string,
			                           *desc, add_configuration_callback, udev);
			break;
		}

		case USB_DEVICE_REMOVE:
		{
			struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);
			if (data) {
				data->dev       = NULL;
				data->kill_task = true;
				lx_emul_task_unblock(data->task);
			} else {
				/* discontinue unclaimed device */
				genode_usb_discontinue_device(udev->bus->busnum, udev->devnum);
			}
			break;
		}
	}

	return NOTIFY_OK;
}


struct notifier_block usb_nb =
{
	.notifier_call = raw_notify
};


static int usbnet_init(void)
{
	int err;

	if ((err = usb_register(&usb_drv)))
		return err;

	usb_register_notify(&usb_nb);
	return 0;
}

/**
 * Let's hook into the usbnet initcall, so we do not need to register
 * an additional one
 */
module_init(usbnet_init);
