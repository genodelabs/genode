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
#include <uapi/linux/usbdevice_fs.h>

#include <lx_emul/shared_dma_buffer.h>
#include <lx_emul/task.h>
#include <lx_emul/usb.h>
#include <lx_user/init.h>
#include <lx_user/io.h>

#include <genode_c_api/usb.h>

struct usb_interface;

struct usb_find_request {
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	struct usb_device  * ret;
};


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

	if (!iface || setting >= iface->num_altsetting)
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

	if (!iface || setting >= iface->num_altsetting)
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

	if (setting >= iface->num_altsetting ||
	    endp    >= iface->altsetting[setting].desc.bNumEndpoints)
		return -3;

	ep = &iface->altsetting[setting].endpoint[endp];
	if (!ep)
		return -4;

	memcpy(buf, &ep->desc,
	       min(sizeof(struct usb_endpoint_descriptor), size));

	return 0;
}


static inline void *
usercontext_from_handle(genode_usb_session_handle_t session_handle,
                        genode_usb_request_handle_t request_handle)
{
	return (void*)(((unsigned long)session_handle << 16) | request_handle);
}


static inline void
handle_from_usercontext(void * usercontext,
                        genode_usb_session_handle_t * session_handle,
                        genode_usb_request_handle_t * request_handle)
{
	*session_handle = (genode_usb_session_handle_t)((unsigned long)usercontext >> 16);
	*request_handle = (genode_usb_request_handle_t)((unsigned long)usercontext & 0xffff);
}


static genode_usb_request_ret_t
handle_return_code(struct genode_usb_request_urb req,
                   struct genode_usb_buffer      payload,
                   void                         *data)
{
	switch (*(int*)data) {
	case 0:          return NO_ERROR;
	case -ENOENT:    return INTERFACE_OR_ENDPOINT_ERROR;
	case -ENOMEM:    return MEMORY_ERROR;
	case -ENODEV:    return NO_DEVICE_ERROR;
	case -ESHUTDOWN: return NO_DEVICE_ERROR;
	case -ENOSPC:    return STALL_ERROR;
	case -EPROTO:    return PROTOCOL_ERROR;
	case -EILSEQ:    return PROTOCOL_ERROR;
	case -EPIPE:     return STALL_ERROR;
	case -ETIMEDOUT: return TIMEOUT_ERROR;
	case -EINVAL:    return PACKET_INVALID_ERROR;
	default:         return UNKNOWN_ERROR;
	};
};


typedef enum usb_rpc_call_type {
	CLAIM,
	RELEASE_IF,
	RELEASE_ALL
} usb_rpc_call_type_t;


struct usb_rpc_call_args {
	unsigned             iface_num;
	usb_rpc_call_type_t  call;
	int                  ret;
};


struct usb_per_dev_data
{
	struct inode              inode;
	struct file               file;
	struct usb_rpc_call_args *rpc;
	struct usb_device        *dev;
	struct task_struct       *task;
	struct usb_anchor         submitted;
};


static int poll_usb_device(void * args);
static struct file * open_usb_dev(struct usb_device * udev)
{
	struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);

	if (!data) {
		int pid;
		data = kmalloc(sizeof(struct usb_per_dev_data), GFP_KERNEL);
		data->inode.i_rdev = udev->dev.devt;
		data->file.f_inode = &data->inode;
		data->file.f_mode  = FMODE_WRITE;
		data->dev = udev;
		pid = kernel_thread(poll_usb_device, data, CLONE_FS | CLONE_FILES);
		data->task = find_task_by_pid_ns(pid, NULL);
		init_usb_anchor(&data->submitted);
		dev_set_drvdata(&udev->dev, data);
	}

	return &data->file;
}


static void release_device(struct usb_per_dev_data * data)
{
	unsigned int ifnum;

	usb_kill_anchored_urbs(&data->submitted);

	if (!data->dev)
		return;

	/**
	 * If the device gets released although it is still available,
	 * reset the device to be sane for new sessions aquiring it
	 */
	for (ifnum = 0; ifnum < 8; ifnum++) {
		struct usb_interface * iface = usb_ifnum_to_if(data->dev, ifnum);
		if (iface) usb_driver_release_interface(&usb_drv, iface);
	}
	usb_reset_device(data->dev);
}


static int claim_iface(struct usb_device *udev, unsigned int ifnum)
{
	struct usb_interface * iface;
	int ret = -ENODEV;

	iface = usb_ifnum_to_if(udev, ifnum);
	if (iface) ret = usb_driver_claim_interface(&usb_drv, iface, NULL);
	return ret;
}


static int release_iface(struct usb_device *udev, unsigned int ifnum)
{
	struct usb_interface * iface = usb_ifnum_to_if(udev, ifnum);
	if (iface) usb_driver_release_interface(&usb_drv, iface);
	return 0;
}


static int usb_rpc_finished(void *d)
{
	struct usb_rpc_call_args * rpc = (struct usb_rpc_call_args*)d;
	return (rpc->ret <= 0);
}


static int claim(genode_usb_bus_num_t bus,
                 genode_usb_dev_num_t dev,
                 unsigned             iface_num)
{
	struct usb_device       * udev = find_usb_device(bus, dev);
	struct usb_per_dev_data * data = udev ? dev_get_drvdata(&udev->dev) : NULL;

	struct usb_rpc_call_args rpc = {
		.ret       = 1,
		.call      = CLAIM,
		.iface_num = iface_num
	};

	/*
	 * As long as 'claim' is a rpc-call, and the usb device wasn't opened yet,
	 * we cannot open the device here, this has to be done from a Linux task.
	 * So just ignore it here.
	 */
	if (!data)
		return 0;

	data->rpc = &rpc;
	lx_emul_task_unblock(data->task);
	lx_emul_execute_kernel_until(&usb_rpc_finished, &rpc);
	return rpc.ret;
}


static int release(genode_usb_bus_num_t bus,
                   genode_usb_dev_num_t dev,
                   unsigned             iface_num)
{
	struct usb_device       * udev = find_usb_device(bus, dev);
	struct usb_per_dev_data * data = udev ? dev_get_drvdata(&udev->dev) : NULL;

	struct usb_rpc_call_args rpc = {
		.ret       = 1,
		.call      = RELEASE_IF,
		.iface_num = iface_num
	};

	if (!data)
		return -1;

	data->rpc = &rpc;
	lx_emul_task_unblock(data->task);
	lx_emul_execute_kernel_until(&usb_rpc_finished, &rpc);
	return rpc.ret;
}


static void release_all(genode_usb_bus_num_t bus,
                        genode_usb_dev_num_t dev)
{
	struct usb_device       * udev = find_usb_device(bus, dev);
	struct usb_per_dev_data * data = udev ? dev_get_drvdata(&udev->dev) : NULL;

	struct usb_rpc_call_args rpc = {
		.ret  = 1,
		.call = RELEASE_ALL,
	};

	if (!data)
		return;

	data->rpc = &rpc;
	lx_emul_task_unblock(data->task);
	lx_emul_execute_kernel_until(&usb_rpc_finished, &rpc);
}


struct genode_usb_rpc_callbacks lx_emul_usb_rpc_callbacks = {
	.alloc_fn        = lx_emul_shared_dma_buffer_allocate,
	.free_fn         = lx_emul_shared_dma_buffer_free,
	.cfg_desc_fn     = config_descriptor,
	.alt_settings_fn = alt_settings,
	.iface_desc_fn   = interface_descriptor,
	.iface_extra_fn  = interface_extra,
	.endp_desc_fn    = endpoint_descriptor,
	.claim_fn        = claim,
	.release_fn      = release,
	.release_all_fn  = release_all,
};


static genode_usb_request_ret_t
handle_transfer_response(struct genode_usb_request_urb req,
                         struct genode_usb_buffer payload,
                         void * data)
{
	struct urb * urb = (struct urb *) data;
	struct genode_usb_request_transfer * transfer =
		genode_usb_get_request_transfer(&req);

	if (urb->status < 0)
		return handle_return_code(req, payload, &urb->status);

	if (transfer) transfer->actual_size = urb->actual_length;

	if (req.type == ISOC) {
		int i;
		struct genode_usb_isoc_transfer *isoc =
			(struct genode_usb_isoc_transfer *)payload.addr;
		if (!isoc)
			return PACKET_INVALID_ERROR;

		for (i = 0; i < urb->number_of_packets; i++) {
			isoc->actual_packet_size[i] =
				urb->iso_frame_desc[i].actual_length;
		}
	}

	return NO_ERROR;
}


static void
handle_string_request(struct genode_usb_request_string * req,
                      genode_usb_session_handle_t        session,
                      genode_usb_request_handle_t        request,
                      struct genode_usb_buffer           payload,
                      void                             * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	int i, ret = -ENODEV;

	if (udev) {
		for (i = 0; i < 3; ++i) {
			/* retry on length 0 or error; some devices are flakey */
			ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
					USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
					(USB_DT_STRING << 8) + req->index, udev->string_langid,
					payload.addr, payload.size,
					USB_CTRL_GET_TIMEOUT);
			if (ret <= 0 && ret != -ETIMEDOUT)
				continue;
			break;
		}
	}

	if (ret > 0) ret = 0;
	genode_usb_ack_request(session, request, handle_return_code, &ret);
}


static void
handle_altsetting_request(unsigned                    iface,
                          unsigned                    alt_setting,
                          genode_usb_session_handle_t session,
                          genode_usb_request_handle_t request,
                          void                      * data)
{
	int ret = -ENODEV;
	struct usb_device * udev = (struct usb_device *) data;

	if (udev)
		ret = usb_set_interface(udev, iface, alt_setting);

	if (ret)
		printk("Alt setting request (iface=%u alt_setting=%u) failed\n",
		       iface, alt_setting);

	genode_usb_ack_request(session, request, handle_return_code, &ret);
}


static void
handle_config_request(unsigned                    cfg_idx,
                      genode_usb_session_handle_t session,
                      genode_usb_request_handle_t request,
                      void                      * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	int ret = udev ? 0 : -ENODEV;

	/*
	 * Skip SET_CONFIGURATION requests if the device already has the
	 * selected config as active config. This workaround prevents issues
	 * with Linux guests in vbox and SDC-reader passthrough.
	 */
	if (udev && !(udev->actconfig &&
	      udev->actconfig->desc.bConfigurationValue == cfg_idx)) {
		ret = usb_set_configuration(udev, cfg_idx);
	}

	genode_usb_ack_request(session, request, handle_return_code, &ret);
}


static void
handle_flush_request(unsigned char               ep,
                     genode_usb_session_handle_t session,
                     genode_usb_request_handle_t request,
                     void                      * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	int ret = udev ? 0 : -ENODEV;
	struct usb_host_endpoint * endpoint;

	if (udev) {
		endpoint = ep & USB_DIR_IN ? udev->ep_in[ep & 0xf]
		              : udev->ep_out[ep & 0xf];
		if (endpoint)
			usb_hcd_flush_endpoint(udev, endpoint);
	}

	genode_usb_ack_request(session, request, handle_return_code, &ret);
}


static genode_usb_request_ret_t
handle_ctrl_response(struct genode_usb_request_urb req,
                         struct genode_usb_buffer payload,
                         void * data)
{
	int status = *(int*)data;
	struct genode_usb_request_control * ctrl =
		genode_usb_get_request_control(&req);

	if (status < 0)
		return handle_return_code(req, payload, &status);

	ctrl->actual_size = status;
	return NO_ERROR;
}


static void async_complete(struct urb *urb)
{
	genode_usb_session_handle_t session_handle;
	genode_usb_request_handle_t request_handle;
	handle_from_usercontext(urb->context, &session_handle, &request_handle);
	genode_usb_ack_request(session_handle, request_handle,
	                       handle_transfer_response, (void*)urb);
	usb_free_urb(urb);
	lx_user_handle_io();
}


static int fill_bulk_urb(struct usb_device                  * udev,
                         struct genode_usb_request_transfer * req,
                         void                               * handle,
                         struct genode_usb_buffer             buf,
                         int                                  read,
                         struct urb                        ** urb)
{
	int pipe = (read)
		? usb_rcvbulkpipe(udev, req->ep) : usb_sndbulkpipe(udev, req->ep);

	if (!buf.addr)
		return -EINVAL;

	*urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!*urb)
		return -ENOMEM;

	usb_fill_bulk_urb(*urb, udev, pipe, buf.addr, buf.size, async_complete, handle);
	return 0;
}


static int fill_irq_urb(struct usb_device                  * udev,
                        struct genode_usb_request_transfer * req,
                        void                               * handle,
                        struct genode_usb_buffer             buf,
                        int                                  read,
                        struct urb                        ** urb)
{
	int polling_interval;
	int pipe = (read)
		? usb_rcvintpipe(udev, req->ep) : usb_sndintpipe(udev, req->ep);

	if (buf.size && !buf.addr)
		return -EINVAL;

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

	usb_fill_int_urb(*urb, udev, pipe, buf.addr, buf.size,
	                 async_complete, handle, polling_interval);
	return 0;
}


static int fill_isoc_urb(struct usb_device                  * udev,
                         struct genode_usb_request_transfer * req,
                         void                               * handle,
                         struct genode_usb_buffer             buf,
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

	struct genode_usb_isoc_transfer *isoc = (struct genode_usb_isoc_transfer *)buf.addr;

	if (!buf.addr || isoc->number_of_packets > MAX_PACKETS)
		return -EINVAL;
	if (!ep)
		return -ENOENT;

	*urb = usb_alloc_urb(isoc->number_of_packets, GFP_KERNEL);
	if (!*urb)
		return -ENOMEM;

	(*urb)->dev                    = udev;
	(*urb)->pipe                   = pipe;
	(*urb)->start_frame            = -1;
	(*urb)->stream_id              = 0;
	(*urb)->transfer_buffer        = isoc->data;
	(*urb)->transfer_buffer_length = buf.size - sizeof(*isoc);
	(*urb)->number_of_packets      = isoc->number_of_packets;
	(*urb)->interval               = 1 << min(15, ep->desc.bInterval - 1);
	(*urb)->context                = handle;
	(*urb)->transfer_flags         = URB_ISO_ASAP | (read ? URB_DIR_IN : URB_DIR_OUT);
	(*urb)->complete               = async_complete;

	for (i = 0; i < isoc->number_of_packets; i++) {
		(*urb)->iso_frame_desc[i].offset = offset;
		(*urb)->iso_frame_desc[i].length = isoc->packet_size[i];
		offset += isoc->packet_size[i];
	}

	return 0;
}


static void
handle_urb_request(struct genode_usb_request_urb req,
                   genode_usb_session_handle_t   session_handle,
                   genode_usb_request_handle_t   request_handle,
                   struct genode_usb_buffer payload, void * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	struct usb_per_dev_data * urbs = dev_get_drvdata(&udev->dev);
	struct genode_usb_request_control * ctrl =
		genode_usb_get_request_control(&req);
	struct genode_usb_request_transfer * transfer =
		genode_usb_get_request_transfer(&req);
	int ret  = 0;
	int read = transfer ? (transfer->ep & 0x80) : 0;
	struct urb * urb = NULL;
	void *handle = usercontext_from_handle(session_handle, request_handle);

	switch (req.type) {
	case CTRL:
		{
			int pipe = (ctrl->request_type & 0x80)
				? usb_rcvctrlpipe(udev, 0) : usb_sndctrlpipe(udev, 0);
			usb_unlock_device(udev);
			ret = usb_control_msg(udev, pipe, ctrl->request, ctrl->request_type,
								  ctrl->value, ctrl->index, payload.addr,
								  payload.size, ctrl->timeout);
			usb_lock_device(udev);
			genode_usb_ack_request(session_handle, request_handle,
			                       handle_ctrl_response, &ret);
			return;
		}
	case BULK:
		ret = fill_bulk_urb(udev, transfer, handle, payload, read, &urb);
		break;
	case IRQ:
		ret = fill_irq_urb(udev, transfer, handle, payload, read, &urb);
		break;
	case ISOC:
		ret = fill_isoc_urb(udev, transfer, handle, payload, read, &urb);
		break;
	default:
		printk("Unknown USB transfer request!\n");
		ret = -EINVAL;
	};

	if (!ret) {
		usb_anchor_urb(urb, &urbs->submitted);
		ret = usb_submit_urb(urb, GFP_KERNEL);
	}

	if (!ret)
		return;

	if (urb) {
		usb_unanchor_urb(urb);
		usb_free_urb(urb);
	}

	genode_usb_ack_request(session_handle, request_handle,
	                       handle_return_code, &ret);
}


static struct genode_usb_request_callbacks request_callbacks = {
	.urb_fn        = handle_urb_request,
	.string_fn     = handle_string_request,
	.altsetting_fn = handle_altsetting_request,
	.config_fn     = handle_config_request,
	.flush_fn      = handle_flush_request,
};


static inline void exit_usb_task(struct usb_per_dev_data * data)
{
	struct usb_device * udev = (struct usb_device *) data->dev;
	release_device(data);
	if (udev) dev_set_drvdata(&udev->dev, NULL);
	kfree(data);
}


static inline void handle_rpc(struct usb_per_dev_data * data)
{
	struct usb_rpc_call_args * rpc = data->rpc;
	if (!rpc)
		return;

	data->rpc = NULL;
	switch(rpc->call) {
	case CLAIM:
		if (data->dev) claim_iface(data->dev, rpc->iface_num);
		rpc->ret = 0;
		return;
	case RELEASE_IF:
		if (data->dev) release_iface(data->dev, rpc->iface_num);
		rpc->ret = 0;
		return;
	case RELEASE_ALL:
		usb_unlock_device(data->dev);
		exit_usb_task(data);
		rpc->ret = 0;
		do_exit(0);
	};
}


static inline int check_for_urb(struct usb_device * udev)
{
	genode_usb_session_handle_t session;

	if (!udev)
		return 0;

	session = genode_usb_session_by_bus_dev(udev->bus->busnum,
	                                        udev->devnum);

	if (!session)
		return 0;

	return genode_usb_request_by_session(session, &request_callbacks,
	                                     (void*)udev);
}


static int poll_usb_device(void * args)
{
	struct usb_per_dev_data * data = (struct usb_per_dev_data*)args;
	genode_usb_bus_num_t      bus  = data->dev->bus->busnum;
	genode_usb_dev_num_t      dev  = data->dev->devnum;

	for (;;) {
		if (data->dev) usb_lock_device(data->dev);
		while (check_for_urb(data->dev)) ;

		/*
		 * we have to check for RPC handling here, during
		 * asynchronous URB handling above the Linux task might
		 * have been blocked and a new RPC entered the entrypoint
		 */
		handle_rpc(data);
		if (data->dev) usb_unlock_device(data->dev);

		/* check if device got removed */
		if (!find_usb_device(bus, dev)) {
			exit_usb_task(data);
			genode_usb_discontinue_device(bus, dev);
			do_exit(0);
		}
		lx_emul_task_schedule(true);
	}

	return 0;
}


static int wake_up_udev_task(struct usb_device *udev, void * args)
{
	struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);

	if (!genode_usb_session_by_bus_dev(udev->bus->busnum, udev->devnum))
		return 0;

	if (!data) {
		open_usb_dev(udev);
		data = dev_get_drvdata(&udev->dev);
	}

	lx_emul_task_unblock(data->task);
	return 0;
}


static int usb_poll_empty_sessions(void * data)
{
	for (;;) {
		usb_for_each_dev(NULL, wake_up_udev_task);
		lx_emul_task_schedule(false);
		genode_usb_handle_empty_sessions();
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
	int pid = kernel_thread(usb_poll_empty_sessions, NULL,
	                        CLONE_FS | CLONE_FILES);
	lx_user_task = find_task_by_pid_ns(pid, NULL);
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
			unsigned long class = 0;
			unsigned i, num = (udev->actconfig) ?
				udev->actconfig->desc.bNumInterfaces : 0;
			for (i = 0; i < num; i++) {
				struct usb_interface * iface =
					(udev->actconfig) ? udev->actconfig->interface[i] : NULL;
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
			struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);
			if (data) {
				data->dev = NULL;
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
