
#include <base/env.h>
#include <usb_session/client.h>

#include <driver.h>
#include <lx_emul.h>

#include <legacy/lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <legacy/lx_emul/extern_c_end.h>

#define TRACE do { ; } while (0)

#include <legacy/lx_emul/impl/kernel.h>
#include <legacy/lx_emul/impl/delay.h>
#include <legacy/lx_emul/impl/work.h>
#include <legacy/lx_emul/impl/spinlock.h>
#include <legacy/lx_emul/impl/mutex.h>
#include <legacy/lx_emul/impl/sched.h>
#include <legacy/lx_emul/impl/timer.h>
#include <legacy/lx_emul/impl/completion.h>
#include <legacy/lx_emul/impl/wait.h>
#include <legacy/lx_emul/impl/usb.h>

#include <legacy/lx_kit/backend_alloc.h>

extern "C" int usb_match_device(struct usb_device *dev,
                                const struct usb_device_id *id)
{
	if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
	    id->idVendor != le16_to_cpu(dev->descriptor.idVendor))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
	    id->idProduct != le16_to_cpu(dev->descriptor.idProduct))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
	    (id->bcdDevice_lo > le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
	    (id->bcdDevice_hi < le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
	    (id->bDeviceClass != dev->descriptor.bDeviceClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
	    (id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
	    (id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
		return 0;

	return 1;
}


extern "C" int usb_match_one_id_intf(struct usb_device *dev,
                                     struct usb_host_interface *intf,
                                     const struct usb_device_id *id)
{
	if (dev->descriptor.bDeviceClass == USB_CLASS_VENDOR_SPEC &&
	    !(id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
	    (id->match_flags & (USB_DEVICE_ID_MATCH_INT_CLASS |
	                        USB_DEVICE_ID_MATCH_INT_SUBCLASS |
	                        USB_DEVICE_ID_MATCH_INT_PROTOCOL |
	                        USB_DEVICE_ID_MATCH_INT_NUMBER)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
	    (id->bInterfaceClass != intf->desc.bInterfaceClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS) &&
	    (id->bInterfaceSubClass != intf->desc.bInterfaceSubClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_PROTOCOL) &&
	    (id->bInterfaceProtocol != intf->desc.bInterfaceProtocol))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_NUMBER) &&
	    (id->bInterfaceNumber != intf->desc.bInterfaceNumber))
		return 0;

	return 1;
}


struct Lx_driver
{
	using Element = Genode::List_element<Lx_driver>;
	using List    = Genode::List<Element>;

	device_driver & dev_drv;
	Element         le { this };

	Lx_driver(device_driver & drv) : dev_drv(drv) { list().insert(&le); }

	bool match(struct device *dev)
	{
		/*
		 *  Don't try if buses don't match, since drivers often use 'container_of'
		 *  which might cast the device to non-matching type
		 */
		if (dev_drv.bus != dev->bus)
			return false;

		return dev_drv.bus->match ? dev_drv.bus->match(dev, &dev_drv)
		                          : false;
	}

	int probe(struct device *dev)
	{
		dev->driver = &dev_drv;
		if (dev_drv.bus->probe) return dev_drv.bus->probe(dev);
		return 0;
	}

	static List & list()
	{
		static List _list;
		return _list;
	}
};


struct task_struct *current;
struct workqueue_struct *system_wq;
unsigned long jiffies;


Genode::Ram_dataspace_capability Lx::backend_alloc(Genode::addr_t size,
                                                   Genode::Cache cache)
{
	return Lx_kit::env().env().ram().alloc(size, cache);
}


const char *dev_name(const struct device *dev) { return dev->name; }


size_t strlen(const char *s) { return Genode::strlen(s); }


int  mutex_lock_interruptible(struct mutex *m)
{
	mutex_lock(m);
	return 0;
}


int driver_register(struct device_driver *drv)
{
	if (!drv)
		return 1;

	Lx_driver * driver = (Lx_driver *)kzalloc(sizeof(Lx_driver), GFP_KERNEL);

	Genode::construct_at<Lx_driver>(driver, *drv);

	return 0;
}

static struct usb_driver * hid_driver = nullptr;
int usb_register_driver(struct usb_driver * driver, struct module *, const char *)
{
	hid_driver = driver;
	return 0;
}


void Driver::Device::probe_interface(usb_interface * iface, usb_device_id * id)
{
	hid_driver->probe(iface, id);
}


void Driver::Device::remove_interface(usb_interface * iface)
{
	hid_driver->disconnect(iface);
	kfree(iface);
}


long __wait_completion(struct completion *work, unsigned long timeout)
{
	Lx::timer_update_jiffies();
	struct process_timer timer { *Lx::scheduler().current() };
	unsigned long        expire = timeout + jiffies;

	if (timeout) {
		timer_setup(&timer.timer, process_timeout, 0);
		mod_timer(&timer.timer, expire);
	}

	while (!work->done) {

		if (timeout && expire <= jiffies) return 0;

		Lx::Task * task = Lx::scheduler().current();
		work->task = (void *)task;
		task->block_and_schedule();
	}

	if (timeout) del_timer(&timer.timer);

	work->done = 0;
	return (expire > jiffies) ? (expire - jiffies) : 1;
}


int dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
	return 0;
}


void *dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}


size_t strlcat(char *dest, const char *src, size_t dest_size)
{
	size_t len_d = strlen(dest);

	if (len_d > dest_size) return 0;

	size_t len = dest_size - len_d - 1;

	memcpy(dest + len_d, src, len);
	dest[len_d + len] = 0;
	return len;
}


int __usb_get_extra_descriptor(char *buffer, unsigned size, unsigned char type, void **ptr)
{
	struct usb_descriptor_header *header;

	while (size >= sizeof(struct usb_descriptor_header)) {
		header = (struct usb_descriptor_header *)buffer;

		if (header->bLength < 2) {
			printk(KERN_ERR
				   "%s: bogus descriptor, type %d length %d\n",
				   usbcore_name,
				   header->bDescriptorType,
				   header->bLength);
			return -1;
		}

		if (header->bDescriptorType == type) {
			*ptr = header;
			return 0;
		}

		buffer += header->bLength;
		size -= header->bLength;
	}

	return -1;
}


void *vzalloc(unsigned long size)
{
	return kzalloc(size, 0);
}


void vfree(void *addr)
{
	if (!addr) return;
	kfree(addr);
}


int device_add(struct device *dev)
{
	if (dev->driver) return 0;

	/* foreach driver match and probe device */
	using Le = Genode::List_element<Lx_driver>;
	for (Le *le = Lx_driver::list().first(); le; le = le->next())
		if (le->object()->match(dev)) {
			int ret = le->object()->probe(dev);
			if (!ret) return 0;
		}

	return 0;
}


void device_del(struct device *dev)
{
	if (dev->bus && dev->bus->remove)
		dev->bus->remove(dev);
}


void *usb_alloc_coherent(struct usb_device *dev, size_t size, gfp_t mem_flags, dma_addr_t *dma)
{
	return kmalloc(size, GFP_KERNEL);
}


struct device *get_device(struct device *dev)
{
	dev->ref++;
	return dev;
}


void put_device(struct device *dev)
{
	if (dev->ref) {
		dev->ref--;
		return;
	}

	if (dev->release)
		dev->release(dev);
	else if (dev->type && dev->type->release)
		dev->type->release(dev);
}


void cdev_init(struct cdev *c, const struct file_operations *fops)
{
	c->ops = fops;
}


void usb_free_coherent(struct usb_device *dev, size_t size, void *addr, dma_addr_t dma)
{
	kfree(addr);
}


int  mutex_lock_killable(struct mutex *lock)
{
	mutex_lock(lock);
	return 0;
}


u16 get_unaligned_le16(const void *p)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *)p;
	return ptr->x;
}


unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	unsigned long i  = offset / BITS_PER_LONG;
	offset -= (i * BITS_PER_LONG);

	for (; offset < size; offset++)
		if (addr[i] & (1UL << offset))
			return offset + (i * BITS_PER_LONG);

	return size;
}


long find_next_zero_bit_le(const void *addr,
                           unsigned long size, unsigned long offset)
{
	unsigned long max_size = sizeof(long) * 8;
	if (offset >= max_size) {
		Genode::warning("Offset greater max size");
		return offset + size;
	}

	for (; offset < max_size; offset++)
		if (!(*(unsigned long*)addr & (1L << offset)))
			return offset;

	return offset + size;
}


u32 get_unaligned_le32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}


void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return kzalloc(size, gfp);
}


int usb_get_descriptor(struct usb_device *dev, unsigned char type,
                       unsigned char index, void *buf, int size)
{
	int i;
	int result;

	memset(buf, 0, size);

	for (i = 0; i < 3; ++i) {
		/* retry on length 0 or error; some devices are flakey */
		result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		                         USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
		                         (type << 8) + index, 0, buf, size,
		                         USB_CTRL_GET_TIMEOUT);
		if (result <= 0 && result != -ETIMEDOUT)
			continue;
		if (result > 1 && ((u8 *)buf)[1] != type) {
			result = -ENODATA;
			continue;
		}
		break;
	}
	return result;
}



static struct usb_interface_assoc_descriptor *find_iad(struct usb_device *dev,
                                                       struct usb_host_config *config,
                                                       u8 inum)
{
	struct usb_interface_assoc_descriptor *retval = NULL;
	struct usb_interface_assoc_descriptor *intf_assoc;
	int first_intf;
	int last_intf;
	int i;

	for (i = 0; (i < USB_MAXIADS && config->intf_assoc[i]); i++) {
		intf_assoc = config->intf_assoc[i];
		if (intf_assoc->bInterfaceCount == 0)
			continue;

		first_intf = intf_assoc->bFirstInterface;
		last_intf = first_intf + (intf_assoc->bInterfaceCount - 1);
		if (inum >= first_intf && inum <= last_intf) {
			if (!retval)
				retval = intf_assoc;
			else
				dev_err(&dev->dev, "Interface #%d referenced"
						" by multiple IADs\n", inum);
		}
	}

	return retval;
}


struct usb_host_interface *usb_altnum_to_altsetting(const struct usb_interface *intf,
                                                    unsigned int altnum)
{
	for (unsigned i = 0; i < intf->num_altsetting; i++) {
		if (intf->altsetting[i].desc.bAlternateSetting == altnum)
			return &intf->altsetting[i];
	}
	return NULL;
}


int usb_set_configuration(struct usb_device *dev, int configuration)
{
	int i, ret;
	struct usb_host_config *cp = NULL;
	struct usb_interface **new_interfaces = NULL;
	int n, nintf;

	if (dev->authorized == 0 || configuration == -1)
		configuration = 0;
	else {
		for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
			if (dev->config[i].desc.bConfigurationValue ==
				configuration) {
				cp = &dev->config[i];
				break;
			}
		}
	}
	if ((!cp && configuration != 0))
		return -EINVAL;

	/* The USB spec says configuration 0 means unconfigured.
	 * But if a device includes a configuration numbered 0,
	 * we will accept it as a correctly configured state.
	 * Use -1 if you really want to unconfigure the device.
	 */
	if (cp && configuration == 0)
		dev_warn(&dev->dev, "config 0 descriptor??\n");

	/* Allocate memory for new interfaces before doing anything else,
	 * so that if we run out then nothing will have changed. */
	n = nintf = 0;
	if (cp) {
		nintf = cp->desc.bNumInterfaces;
		new_interfaces = (struct usb_interface **)
			kmalloc(nintf * sizeof(*new_interfaces), GFP_KERNEL);
		if (!new_interfaces)
			return -ENOMEM;

		for (; n < nintf; ++n) {
			new_interfaces[n] = (struct usb_interface*)
				kzalloc( sizeof(struct usb_interface), GFP_KERNEL);
			if (!new_interfaces[n]) {
				ret = -ENOMEM;
				while (--n >= 0)
					kfree(new_interfaces[n]);
				kfree(new_interfaces);
				return ret;
			}
		}
	}

	/*
	 * Initialize the new interface structures and the
	 * hc/hcd/usbcore interface/endpoint state.
	 */
	for (i = 0; i < nintf; ++i) {
		struct usb_interface_cache *intfc;
		struct usb_interface *intf;
		struct usb_host_interface *alt;
		u8 ifnum;

		cp->interface[i] = intf = new_interfaces[i];
		intfc = cp->intf_cache[i];
		intf->altsetting = intfc->altsetting;
		intf->num_altsetting = intfc->num_altsetting;
		intf->authorized = 1; //FIXME

		alt = usb_altnum_to_altsetting(intf, 0);

		/* No altsetting 0?  We'll assume the first altsetting.
		 * We could use a GetInterface call, but if a device is
		 * so non-compliant that it doesn't have altsetting 0
		 * then I wouldn't trust its reply anyway.
		 */
		if (!alt)
			alt = &intf->altsetting[0];

		ifnum = alt->desc.bInterfaceNumber;
		intf->intf_assoc = find_iad(dev, cp, ifnum);
		intf->cur_altsetting = alt;
		intf->dev.parent = &dev->dev;
		intf->dev.driver = NULL;
		intf->dev.bus = (bus_type*) 0xdeadbeef /*&usb_bus_type*/;
		intf->minor = -1;
		device_initialize(&intf->dev);
		dev_set_name(&intf->dev, "%d-%s:%d.%d", dev->bus->busnum,
					 dev->devpath, configuration, ifnum);
	}
	kfree(new_interfaces);

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
						  USB_REQ_SET_CONFIGURATION, 0, configuration, 0,
						  NULL, 0, USB_CTRL_SET_TIMEOUT);
	if (ret < 0 && cp) {
		for (i = 0; i < nintf; ++i) {
			put_device(&cp->interface[i]->dev);
			cp->interface[i] = NULL;
		}
		cp = NULL;
	}

	dev->actconfig = cp;

	if (!cp) {
		dev->state = USB_STATE_ADDRESS;
		return ret;
	}
	dev->state = USB_STATE_CONFIGURED;

	return 0;
}

static Genode::Allocator &heap()
{
	static Genode::Heap heap(Lx_kit::env().env().ram(), Lx_kit::env().env().rm());
	return heap;
}

extern "C"
void *kmalloc(size_t size, gfp_t flags)
{
	try {
		void * const addr = heap().alloc(size);

		if (!addr)
			return 0;

		if ((Genode::addr_t)addr & 0x3)
			Genode::error("unaligned kmalloc ", (Genode::addr_t)addr);

		if (flags & __GFP_ZERO)
			memset(addr, 0, size);

		return addr;
	} catch (...) {
		return NULL;
	}
}

extern "C"
void kfree(const void *p)
{
	if (!p) return;

	heap().free((void *)p, 0);
}

extern "C"
void *kzalloc(size_t size, gfp_t flags)
{
	return kmalloc(size, flags | __GFP_ZERO);
}

extern "C"
void *kcalloc(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > (~0UL / size))
		return 0;

	return kzalloc(n * size, flags);
}

extern "C"
void *kmemdup(const void *src, size_t size, gfp_t flags)
{
	void *addr = kmalloc(size, flags);

	if (addr)
		memcpy(addr, src, size);

	return addr;
}

/******************
 ** linux/kref.h **
 ******************/

void kref_init(struct kref *kref)
{ 
	atomic_set(&kref->refcount, 1);
}


void kref_get(struct kref *kref)
{
	atomic_inc(&kref->refcount);
}


int  kref_put(struct kref *kref, void (*release) (struct kref *kref))
{
	if(!atomic_dec_return(&kref->refcount)) {
		release(kref);
		return 1;
	}
	return 0;
}

