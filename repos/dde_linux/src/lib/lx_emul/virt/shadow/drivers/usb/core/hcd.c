#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <genode_c_api/usb_client.h>
#include <lx_emul/usb_client.h>

/* wait queue for synchronous unlinks */
DECLARE_WAIT_QUEUE_HEAD(usb_kill_urb_queue);

extern void lx_user_handle_io(void);

int usb_hcd_submit_urb (struct urb *urb, gfp_t mem_flags)
{
	genode_usb_client_dev_handle_t handle;
	genode_usb_client_ret_val_t r = INVALID;

	/* increment urb's reference count as part of giving it to the HCD
	 * (which will control it).  HCD guarantees that it either returns
	 * an error or calls giveback(), but not both.
	 */
	usb_get_urb(urb);
	atomic_inc(&urb->use_count);
	atomic_inc(&urb->dev->urbnum);

	if (!urb->dev->bus)
		return -ENODEV;

	handle = (genode_usb_client_dev_handle_t)urb->dev->filelist.prev;

	switch(usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		{
			struct usb_ctrlrequest * ctrl = (struct usb_ctrlrequest *)
				urb->setup_packet;
			r = genode_usb_client_device_control(handle,
			                                     ctrl->bRequest,
			                                     ctrl->bRequestType,
			                                     ctrl->wValue,
			                                     ctrl->wIndex,
			                                     urb->transfer_buffer_length,
			                                     urb);
			break;
		}
	case PIPE_INTERRUPT:
		{
			r = genode_usb_client_iface_transfer(handle, IRQ,
			                                     urb->ep->desc.bEndpointAddress,
			                                     urb->transfer_buffer_length,
			                                     urb);
			break;
		}
	case PIPE_BULK:
			r = genode_usb_client_iface_transfer(handle, BULK,
			                                     urb->ep->desc.bEndpointAddress,
			                                     urb->transfer_buffer_length,
			                                     urb);
			break;
	default:
		printk("unknown URB requested: %d\n", usb_pipetype(urb->pipe));
	}

	switch (r) {
	case NO_DEVICE: return -ENODEV;
	case NO_MEMORY: return -ENOMEM;
	case HALT:      return -EPIPE;
	case INVALID:   return -EINVAL;
	case TIMEOUT:   return -ETIMEDOUT;
	case OK:        break;
	};

	lx_emul_usb_client_ticker();
	return 0;
}


int usb_hcd_unlink_urb (struct urb *urb, int status)
{
	struct usb_device *udev = urb->dev;
	int ret = -EIDRM;

	if (atomic_read(&urb->use_count) > 0 &&
	    udev->state == USB_STATE_NOTATTACHED) {
		ret = 0;
		atomic_set(&urb->use_count, 0);
	}

	return ret;
}
