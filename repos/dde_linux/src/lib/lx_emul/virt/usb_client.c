#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <../drivers/usb/core/usb.h>

#include <lx_emul/usb_client.h>
#include <genode_c_api/usb_client.h>


struct meta_data
{
	struct usb_hcd    *hcd;
	struct device     *sysdev;
	struct usb_device *udev;
};

static struct usb_hcd * dummy_hc_device(void)
{
	static int initialized = 0;
	static struct hc_driver _hc_driver = { };
	static struct usb_hcd hcd;
	static struct device  sysdev;
	static struct mutex   address0_mutex;
	static struct mutex   bandwidth_mutex;

	if (!initialized) {
		device_initialize(&sysdev);
		mutex_init(&address0_mutex);
		mutex_init(&bandwidth_mutex);
		kref_init(&hcd.kref);
		hcd.driver          = &_hc_driver;
		hcd.self.bus_name   = "usbbus";
		hcd.self.sysdev     = &sysdev;
		hcd.dev_policy      = USB_DEVICE_AUTHORIZE_ALL;
		hcd.address0_mutex  = &address0_mutex;
		hcd.bandwidth_mutex = &bandwidth_mutex;
		set_bit(HCD_FLAG_HW_ACCESSIBLE,   &hcd.flags);
		set_bit(HCD_FLAG_INTF_AUTHORIZED, &hcd.flags);
		initialized = 1;
	}
	return &hcd;
}

static void * register_device(genode_usb_client_dev_handle_t handle,
                              char const *label, genode_usb_speed_t speed)
{
	int err;
	static int num = 0;
	struct usb_device * udev;
	struct usb_device_descriptor *descr = NULL;

	udev = usb_alloc_dev(NULL, &dummy_hc_device()->self, 0);
	if (!udev) {
		printk("error: could not allocate udev for %s\n", label);
		return NULL;
	}

	/*
	 * We store handle in filelist list head to be used in hcd urb submission
	 * before sending any URB. The filelist member is referenced in devio.c
	 * only which is not used here.
	 */
	udev->filelist.prev = (struct list_head *) handle;

	udev->devnum = num++;

	switch (speed) {
	case GENODE_USB_SPEED_LOW:   udev->speed = USB_SPEED_LOW;   break;
	case GENODE_USB_SPEED_FULL:  udev->speed = USB_SPEED_FULL;  break;
	case GENODE_USB_SPEED_HIGH:  udev->speed = USB_SPEED_HIGH;  break;
	case GENODE_USB_SPEED_SUPER: udev->speed = USB_SPEED_SUPER; break;
	case GENODE_USB_SPEED_SUPER_PLUS:
		udev->speed = USB_SPEED_SUPER_PLUS;
		udev->ssp_rate = USB_SSP_GEN_2x1;
		break;
	case GENODE_USB_SPEED_SUPER_PLUS_2X2:
		udev->speed = USB_SPEED_SUPER_PLUS;
		udev->ssp_rate = USB_SSP_GEN_2x2;
		break;
	default:
		udev->speed = USB_SPEED_FULL;
		break;
	};

	udev->authorized = 1;
	udev->bus_mA     = 900; /* set to maximum USB3.0 */
	usb_set_device_state(udev, USB_STATE_ADDRESS);

	dev_set_name(&udev->dev, "%s", label);
	device_set_wakeup_capable(&udev->dev, 1);
	udev->ep0.desc.wMaxPacketSize = cpu_to_le16(64);

	descr = usb_get_device_descriptor(udev);
	if (PTR_ERR(descr) < 0) {
		dev_err(&udev->dev, "can't read device descriptor: %ld\n", PTR_ERR(descr));
		kfree(descr);
		usb_put_dev(udev);
		return NULL;
	}

	udev->descriptor = *descr;
	kfree(descr);

	err = usb_new_device(udev);
	if (err) {
		printk("error: usb_new_device failed %d\n", err);
		usb_put_dev(udev);
		return NULL;
	}

	return udev;
}


static void urb_out(void * data, genode_buffer_t buf)
{
	struct urb *urb = (struct urb *) data;
	memcpy(buf.addr, urb->transfer_buffer,
	       urb->transfer_buffer_length);
}


static void urb_in(void * data, genode_buffer_t buf)
{
	struct urb *urb = (struct urb *) data;
	memcpy(urb->transfer_buffer, buf.addr, buf.size);
	urb->actual_length = buf.size;
}


static genode_uint32_t isoc_urb_out(void * data, genode_uint32_t idx,
                                    genode_buffer_t buf)
{
	printk("%s: not implemented yet, we had no isochronous Linux driver yet",
	       __func__);
	return 0;
}


static void isoc_urb_in(void * data, genode_uint32_t idx, genode_buffer_t buf)
{
	printk("%s: not implemented yet, we had no isochronous Linux driver yet",
	       __func__);
}


static void urb_complete(void * data, genode_usb_client_ret_val_t result)
{
	struct urb *urb = (struct urb *) data;
	switch (result) {
	case OK:        urb->status = 0;          break;
	case NO_DEVICE: urb->status = -ENOENT;    break;
	case NO_MEMORY: urb->status = -ENOMEM;    break;
	case HALT:      urb->status = -EPIPE;     break;
	case INVALID:   urb->status = -EINVAL;    break;
	case TIMEOUT:   urb->status = -ETIMEDOUT;
	};
	if (urb->complete) urb->complete(urb);

	atomic_dec(&urb->use_count);
	usb_put_urb(urb);
}


static void unregister_device(genode_usb_client_dev_handle_t handle, void *data)
{
	struct usb_device *udev = (struct usb_device *)data;
	genode_usb_client_device_update(urb_out, urb_in, isoc_urb_out,
	                                isoc_urb_in, urb_complete);

	/* inform driver about ongoing unregister before disconnection */
	lx_emul_usb_client_device_unregister_callback(udev);

	udev->filelist.prev = NULL;
	usb_disconnect(&udev);
	usb_put_dev(udev);
}


static int usb_loop(void *arg)
{
	for (;;) {
		genode_usb_client_device_update(urb_out, urb_in, isoc_urb_out,
		                                isoc_urb_in, urb_complete);
		lx_emul_task_schedule(true);
	}
	return -1;
}


static struct task_struct * usb_task = NULL;


static int usb_rom_loop(void *arg)
{
	for (;;) {

		genode_usb_client_update(register_device, unregister_device);

		/* block until lx_emul_task_unblock */
		lx_emul_task_schedule(true);
	}
	return 0;
}


static struct task_struct *usb_rom_task = NULL;


void lx_emul_usb_client_init(void)
{
	pid_t pid;

	pid = kernel_thread(usb_rom_loop, NULL, "usb_rom_task", CLONE_FS | CLONE_FILES);
	usb_rom_task = find_task_by_pid_ns(pid, NULL);

	pid = kernel_thread(usb_loop, NULL, "usb_task", CLONE_FS | CLONE_FILES);
	usb_task = find_task_by_pid_ns(pid, NULL);
}


void lx_emul_usb_client_rom_update(void)
{
	if (usb_rom_task) lx_emul_task_unblock(usb_rom_task);
}


void lx_emul_usb_client_ticker(void)
{
	if (usb_task) lx_emul_task_unblock(usb_task);
}
