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

static struct file_operations * usbdev_file_operations = NULL;

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
	case -EPROTO:    return PROTOCOL_ERROR;
	case -EILSEQ:    return PROTOCOL_ERROR;
	case -EPIPE:     return STALL_ERROR;
	case -ETIMEDOUT: return TIMEOUT_ERROR;
	default:         return UNKNOWN_ERROR;
	};
};


typedef enum usb_rpc_call_type {
	NOTHING,
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
	struct inode             inode;
	struct file              file;
	struct usb_rpc_call_args rpc;
	struct usb_device       *dev;
	struct task_struct      *task;
	struct list_head         urblist;
};


struct usb_urb_in_flight
{
	struct list_head    le;
	struct usbdevfs_urb urb;
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
		usbdev_file_operations->open(&data->inode, &data->file);
		data->dev = udev;
		pid = kernel_thread(poll_usb_device, data, CLONE_FS | CLONE_FILES);
		data->task = find_task_by_pid_ns(pid, NULL);
		INIT_LIST_HEAD(&data->urblist);
		dev_set_drvdata(&udev->dev, data);
	}

	return &data->file;
}


static void release_device(struct usb_per_dev_data * data, int reset)
{
	genode_usb_session_handle_t session;
	genode_usb_request_handle_t request;
	struct usb_urb_in_flight   *iter, *next;
	int ret = -ENODEV;

	list_for_each_entry_safe(iter, next, &data->urblist, le) {
		usbdev_file_operations->unlocked_ioctl(&data->file,
		                                       USBDEVFS_DISCARDURB,
		                                       (unsigned long)&iter->urb);
		handle_from_usercontext(iter->urb.usercontext, &session, &request);
		genode_usb_ack_request(session, request, handle_return_code, &ret);
		list_del(&iter->le);
		kfree(iter);
	}
	usbdev_file_operations->release(&data->inode, &data->file);
	if (reset)
		usbdev_file_operations->unlocked_ioctl(&data->file, USBDEVFS_RESET, 0);
}


static int claim_iface(struct usb_device *udev, unsigned int ifnum)
{
	struct file * file = open_usb_dev(udev);
	return usbdev_file_operations->unlocked_ioctl(file, USBDEVFS_CLAIMINTERFACE,
	                                              (unsigned long)&ifnum);
}


static int release_iface(struct usb_device *udev, unsigned int ifnum)
{
	struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);

	if (!data)
		return -ENODEV;

	if (data)
		usbdev_file_operations->unlocked_ioctl(&data->file,
		                                       USBDEVFS_RELEASEINTERFACE,
		                                       (unsigned long)&ifnum);
	return 0;
}


static int usb_rpc_finished(void *d)
{
	struct usb_per_dev_data * data = (struct usb_per_dev_data*)d;
	return (data->rpc.ret <= 0);
}


static int claim(genode_usb_bus_num_t bus,
                 genode_usb_dev_num_t dev,
                 unsigned             iface_num)
{
	struct usb_device       * udev = find_usb_device(bus, dev);
	struct usb_per_dev_data * data = udev ? dev_get_drvdata(&udev->dev) : NULL;

	/*
	 * As long as 'claim' is a rpc-call, and the usb device wasn't opened yet,
	 * we cannot open the device here, this has to be done from a Linux task.
	 * So just ignore it here, it will be claimed implicitely by the devio
	 * usb layer later.
	 */
	if (!data)
		return 0;

	data                = dev_get_drvdata(&udev->dev);
	data->rpc.ret       = 1;
	data->rpc.call      = CLAIM;
	data->rpc.iface_num = iface_num;
	lx_emul_task_unblock(data->task);
	lx_emul_execute_kernel_until(&usb_rpc_finished, data);
	return data->rpc.ret;
}


static int release(genode_usb_bus_num_t bus,
                   genode_usb_dev_num_t dev,
                   unsigned             iface_num)
{
	struct usb_device       * udev = find_usb_device(bus, dev);
	struct usb_per_dev_data * data = udev ? dev_get_drvdata(&udev->dev) : NULL;

	if (!data)
		return -1;

	data->rpc.ret       = 1;
	data->rpc.call      = RELEASE_IF;
	data->rpc.iface_num = iface_num;
	lx_emul_task_unblock(data->task);
	lx_emul_execute_kernel_until(&usb_rpc_finished, data);
	return data->rpc.ret;
}


static void release_all(genode_usb_bus_num_t bus,
                        genode_usb_dev_num_t dev)
{
	struct usb_device       * udev = find_usb_device(bus, dev);
	struct usb_per_dev_data * data = udev ? dev_get_drvdata(&udev->dev) : NULL;

	if (!data)
		return;

	data->rpc.ret  = 1;
	data->rpc.call = RELEASE_ALL;
	lx_emul_task_unblock(data->task);
	lx_emul_execute_kernel_until(&usb_rpc_finished, data);
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
	struct usbdevfs_urb * urb = ((struct usbdevfs_urb *)data);
	struct genode_usb_request_control * ctrl =
		genode_usb_get_request_control(&req);
	struct genode_usb_request_transfer * transfer =
		genode_usb_get_request_transfer(&req);

	if (urb->status < 0)
		return handle_return_code(req, payload, &urb->status);

	if (ctrl)     ctrl->actual_size     = urb->actual_length;
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
	struct file       * file = open_usb_dev(udev);

	struct usbdevfs_ctrltransfer ctrl = {
		.bRequestType = USB_DIR_IN,
		.bRequest     = USB_REQ_GET_DESCRIPTOR,
		.wValue       = (USB_DT_STRING << 8) + req->index,
		.wIndex       = udev->string_langid,
		.wLength      = payload.size,
		.timeout      = USB_CTRL_GET_TIMEOUT,
		.data         = payload.addr,
	};
	int ret =
		usbdev_file_operations->unlocked_ioctl(file, USBDEVFS_CONTROL,
	                                           (unsigned long)&ctrl);
	genode_usb_ack_request(session, request, handle_return_code, &ret);
}


static void
handle_altsetting_request(unsigned                    iface,
                          unsigned                    alt_setting,
                          genode_usb_session_handle_t session,
                          genode_usb_request_handle_t request,
                          void                      * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	struct file       * file = open_usb_dev(udev);

	struct usbdevfs_setinterface setintf = {
		.interface  = iface,
		.altsetting = alt_setting
	};

	int ret =
		usbdev_file_operations->unlocked_ioctl(file, USBDEVFS_SETINTERFACE,
		                                       (unsigned long)&setintf);
	genode_usb_ack_request(session, request, handle_return_code, &ret);
}


static void
handle_config_request(unsigned                    cfg_idx,
                      genode_usb_session_handle_t session,
                      genode_usb_request_handle_t request,
                      void                      * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	struct file       * file = open_usb_dev(udev);

	int u   = cfg_idx;
	int ret =
		usbdev_file_operations->unlocked_ioctl(file,
		                                       USBDEVFS_SETCONFIGURATION,
		                                       (unsigned long)&u);
	genode_usb_ack_request(session, request, handle_return_code, &ret);
}


static void
handle_flush_request(unsigned char               ep,
                     genode_usb_session_handle_t session,
                     genode_usb_request_handle_t request,
                     void                      * data)
{
	struct usb_device * udev = (struct usb_device *) data;
	struct file       * file = open_usb_dev(udev);
	unsigned int e = ep;
	int ret =
		usbdev_file_operations->unlocked_ioctl(file,
		                                       USBDEVFS_RESETEP,
		                                       (unsigned long)&e);
	genode_usb_ack_request(session, request, handle_return_code, &ret);
}


static void
handle_urb_request(struct genode_usb_request_urb req,
                   genode_usb_session_handle_t   session_handle,
                   genode_usb_request_handle_t   request_handle,
                   struct genode_usb_buffer payload, void * args)
{
	struct usb_device       * udev = (struct usb_device *) args;
	struct usb_per_dev_data * data = dev_get_drvdata(&udev->dev);
	struct file             * file = &data->file;

	switch (req.type) {
	case CTRL:
		{
			struct usbdevfs_urb urb;
			struct genode_usb_request_control * urc =
				genode_usb_get_request_control(&req);
			struct usbdevfs_ctrltransfer ctrl = {
				.bRequestType = urc->request_type,
				.bRequest     = urc->request,
				.wValue       = urc->value,
				.wIndex       = urc->index,
				.wLength      = payload.size,
				.timeout      = urc->timeout,
				.data         = payload.addr,
			};
			urb.actual_length =
				usbdev_file_operations->unlocked_ioctl(file, USBDEVFS_CONTROL,
			                                           (unsigned long)&ctrl);
			urb.status = urb.actual_length < 0 ? urb.actual_length : 0;
			genode_usb_ack_request(session_handle, request_handle,
			                       handle_transfer_response, &urb);
			break;
		}
	case IRQ:
	case BULK:
		{
			struct genode_usb_request_transfer * urt =
				genode_usb_get_request_transfer(&req);
			struct usb_urb_in_flight * u =
				kmalloc(sizeof(struct usb_urb_in_flight), GFP_KERNEL);
			u->urb.type          = req.type == IRQ ? USBDEVFS_URB_TYPE_INTERRUPT
			                                       : USBDEVFS_URB_TYPE_BULK,
			u->urb.endpoint      = urt->ep;
			u->urb.buffer_length = payload.size;
			u->urb.buffer        = payload.addr;
			u->urb.usercontext   = usercontext_from_handle(session_handle,
			                                             request_handle);
			u->urb.signr         = 1;
			INIT_LIST_HEAD(&u->le);
			list_add_tail(&u->le, &data->urblist);
			usbdev_file_operations->unlocked_ioctl(file, USBDEVFS_SUBMITURB,
			                                       (unsigned long)&u->urb);
			break;
		}
	case ISOC:
		{
			int i;
			struct genode_usb_request_transfer * urt =
				genode_usb_get_request_transfer(&req);
			struct genode_usb_isoc_transfer *isoc =
				(struct genode_usb_isoc_transfer *)payload.addr;
			unsigned num_packets = isoc ? isoc->number_of_packets : 0;
			struct usb_urb_in_flight * u =
				kmalloc(sizeof(struct usb_urb_in_flight) +
				        sizeof(struct usbdevfs_iso_packet_desc)*num_packets,
				        GFP_KERNEL);
			u->urb.type              = USBDEVFS_URB_TYPE_ISO,
			u->urb.endpoint          = urt->ep;
			u->urb.buffer_length     = payload.size -
			                           sizeof(struct genode_usb_isoc_transfer);
			u->urb.buffer            = isoc ? isoc->data : NULL;;
			u->urb.usercontext       = usercontext_from_handle(session_handle,
			                                                   request_handle);
			u->urb.signr             = 1;
			u->urb.number_of_packets = num_packets;
			INIT_LIST_HEAD(&u->le);
			list_add_tail(&u->le, &data->urblist);
			for (i = 0; i < num_packets; i++)
				u->urb.iso_frame_desc[i].length = isoc ? isoc->packet_size[i] : 0;
			usbdev_file_operations->unlocked_ioctl(file, USBDEVFS_SUBMITURB,
			                                       (unsigned long)&u->urb);
			break;
		}
	case NONE: ;
	};
}


static struct genode_usb_request_callbacks request_callbacks = {
	.urb_fn        = handle_urb_request,
	.string_fn     = handle_string_request,
	.altsetting_fn = handle_altsetting_request,
	.config_fn     = handle_config_request,
	.flush_fn      = handle_flush_request,
};


static inline void handle_rpc(struct usb_per_dev_data * data)
{
	switch(data->rpc.call) {
	case CLAIM:
		if (data->dev) claim_iface(data->dev, data->rpc.iface_num);
		break;
	case RELEASE_IF:
		if (data->dev) release_iface(data->dev, data->rpc.iface_num);
		break;
	case RELEASE_ALL:
		data->dev = NULL;
		break;
	case NOTHING: ;
	};

	data->rpc.call = NOTHING;
	data->rpc.ret = 0;
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


static inline int check_for_urb_done(struct file * file)
{
	genode_usb_session_handle_t session_handle;
	genode_usb_request_handle_t request_handle;
	struct usb_urb_in_flight * u;
	struct usbdevfs_urb      * urb = NULL;
	int ret = usbdev_file_operations->unlocked_ioctl(file,
	                                                 USBDEVFS_REAPURBNDELAY,
	                                                 (unsigned long)&urb);
	if (ret < 0)
		return 0;

	handle_from_usercontext(urb->usercontext, &session_handle, &request_handle);
	genode_usb_ack_request(session_handle, request_handle,
	                       handle_transfer_response, urb);
	u = container_of(urb, struct usb_urb_in_flight, urb);
	list_del(&u->le);
	kfree(u);
	return 1;
}


static int poll_usb_device(void * args)
{
	struct usb_per_dev_data * data = (struct usb_per_dev_data*)args;
	genode_usb_bus_num_t      bus  = data->dev->bus->busnum;
	genode_usb_dev_num_t      dev  = data->dev->devnum;

	for (;;) {
		handle_rpc(data);
		while (check_for_urb(data->dev)) ;
		while (check_for_urb_done(&data->file)) ;
		if (!data->dev) {
			struct usb_device * udev = find_usb_device(bus, dev);
			release_device(data, udev ? 1 : 0);
			if (!udev) genode_usb_discontinue_device(bus, dev);
			else       dev_set_drvdata(&udev->dev, NULL);
			kfree(data);
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


void lx_emul_usb_register_devio(const struct file_operations * fops)
{
	usbdev_file_operations = fops;
}


#include <linux/sched/signal.h>

int kill_pid_usb_asyncio(int sig, int errno, sigval_t addr, struct pid * pid,
                         const struct cred * cred)
{
	lx_user_handle_io();
	return 0;
}
