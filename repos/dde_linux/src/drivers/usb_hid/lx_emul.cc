
#include <base/env.h>
#include <usb_session/client.h>

#include <driver.h>
#include <lx_emul.h>

#include <lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <lx_emul/extern_c_end.h>

#define TRACE do { ; } while (0)

#include <lx_emul/impl/kernel.h>
#include <lx_emul/impl/delay.h>
#include <lx_emul/impl/slab.h>
#include <lx_emul/impl/work.h>
#include <lx_emul/impl/spinlock.h>
#include <lx_emul/impl/mutex.h>
#include <lx_emul/impl/sched.h>
#include <lx_emul/impl/timer.h>
#include <lx_emul/impl/completion.h>
#include <lx_emul/impl/wait.h>
#include <lx_emul/impl/usb.h>

#include <lx_kit/backend_alloc.h>

struct Lx_driver
{
	using Element = Genode::List_element<Lx_driver>;
	using List    = Genode::List<Element>;

	device_driver & dev_drv;
	Element         le { this };

	Lx_driver(device_driver & drv) : dev_drv(drv) { list().insert(&le); }

	bool match(struct device *dev) {
		return dev_drv.bus->match ? dev_drv.bus->match(dev, &dev_drv)
		                          : false; }

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

Genode::Ram_dataspace_capability Lx::backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached) {
	return Lx_kit::env().env().ram().alloc(size, cached); }

const char *dev_name(const struct device *dev) { return dev->name; }

size_t strlen(const char *s) { return Genode::strlen(s); }

int  mutex_lock_interruptible(struct mutex *m)
{
	mutex_lock(m);
	return 0;
}

int driver_register(struct device_driver *drv)
{
	if (drv) new (Lx::Malloc::mem()) Lx_driver(*drv);
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
	for (unsigned i = 0; i < iface->num_altsetting; i++) {
		if (iface->altsetting[i].extra)
			kfree(iface->altsetting[i].extra);
		kfree(iface->altsetting[i].endpoint);
		kfree(iface->altsetting);
	}
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
	size_t *addr;
	try { addr = (size_t *)Lx::Malloc::mem().alloc_large(size); }
	catch (...) { return 0; }

	memset(addr, 0, size);

	return addr;
}


void vfree(void *addr)
{
	if (!addr) return;
	Lx::Malloc::mem().free_large(addr);
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
	//dev->ref++;
	return dev;
}


void cdev_init(struct cdev *c, const struct file_operations *fops)
{
	c->ops = fops;
}


void usb_free_coherent(struct usb_device *dev, size_t size, void *addr, dma_addr_t dma)
{
	//kfree(dev);
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
