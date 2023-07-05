#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include <lx_emul/usb_client.h>

static struct hc_driver _hc_driver = { };

struct meta_data
{
	struct usb_hcd    *hcd;
	struct device     *sysdev;
	struct usb_device *udev;
};


void *lx_emul_usb_client_register_device(genode_usb_client_handle_t handle, char const *label)
{
	struct meta_data  *meta;
	int err;
	struct genode_usb_device_descriptor dev_descr;
	struct genode_usb_config_descriptor conf_descr;

	err = genode_usb_client_config_descriptor(handle, &dev_descr, &conf_descr);
	if (err) {
		printk("error: failed to read config descriptor\n");
		return NULL;;
	}

	meta = (struct meta_data *)kmalloc(sizeof(struct meta_data), GFP_KERNEL);
	if (!meta) return NULL;

	meta->sysdev = (struct device*)kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!meta->sysdev) goto sysdev;

	device_initialize(meta->sysdev);

	meta->hcd = (struct usb_hcd *)kzalloc(sizeof(struct usb_hcd), GFP_KERNEL);
	if (!meta->hcd) goto hcd;

	/* hcd->self is usb_bus */
	meta->hcd->driver        = &_hc_driver;
	meta->hcd->self.bus_name = "usbbus";
	meta->hcd->self.sysdev   = meta->sysdev;

	meta->udev = usb_alloc_dev(NULL, &meta->hcd->self, 0);
	if (!meta->udev) {
		printk("error: could not allocate udev for %s\n", label);
		goto udev;
	}

	/* usb_alloc_dev sets parent to bus->controller if first argument is NULL */
	meta->hcd->self.controller = (struct device *)handle;

	memcpy(&meta->udev->descriptor, &dev_descr, sizeof(struct usb_device_descriptor));
	meta->udev->devnum     = dev_descr.num;
	meta->udev->speed      = (enum usb_device_speed)dev_descr.speed;
	meta->udev->authorized = 1;
	meta->udev->bus_mA     = 900; /* set to maximum USB3.0 */
	meta->udev->state      = USB_STATE_NOTATTACHED;

	dev_set_name(&meta->udev->dev, "%s", label);

	err = usb_new_device(meta->udev);

	if (err) {
		printk("error: usb_new_device failed %d\n", err);
		goto new_device;
	}
	return meta;

new_device:
	usb_put_dev(meta->udev);
udev:
	kfree(meta->hcd);
hcd:
	kfree(meta->sysdev);
sysdev:
	kfree(meta);

	return NULL;
}


void lx_emul_usb_client_unregister_device(genode_usb_client_handle_t handle, void *data)
{
	struct meta_data *meta = (struct meta_data *)data;

	usb_disconnect(&meta->udev);
	usb_put_dev(meta->udev);
	kfree(meta->hcd);
	kobject_put(&meta->sysdev->kobj);
	kfree(meta->sysdev);
	kfree(meta);
}


#define for_each_claimed_interface(udev, num, intf) \
	for (num = 0, intf = udev->actconfig->interface[num]; \
	     num < udev->actconfig->desc.bNumInterfaces; \
	     intf = udev->actconfig->interface[++num]) \
		if (!usb_interface_claimed(intf)) continue; else


int lx_emul_usb_client_set_configuration(genode_usb_client_handle_t handle, void *data, unsigned long config)
{
	struct meta_data *meta  = (struct meta_data *)data;
	struct usb_device *udev = meta->udev;
	unsigned i = 0;
	struct usb_interface *intf;

	/* release claimed interfaces at session */
	for_each_claimed_interface(udev, i, intf) {
		genode_usb_client_release_interface(handle, i);
	}

	/*
	 * Release claimed interfaces at driver, 'usb_driver_release_interface' may
	 * release more than one interface
	 */
	for_each_claimed_interface(udev, i, intf) {
		usb_driver_release_interface(to_usb_driver(intf->dev.driver), intf);
	}

	/* set unconfigured (internal reset in host driver) first */
	usb_set_configuration(meta->udev, 0);
	return usb_set_configuration(meta->udev, config);
}


void * hcd_buffer_alloc(struct usb_bus * bus, size_t size, gfp_t mem_flags, dma_addr_t * dma)
{
	return kmalloc(size, GFP_KERNEL);
}


void hcd_buffer_free(struct usb_bus * bus, size_t size, void * addr, dma_addr_t dma)
{
	kfree(addr);
}
