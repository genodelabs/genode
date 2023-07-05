#include <base/env.h>
#include <usb_session/client.h>
#include <format/snprintf.h>
#include <nic_session/nic_session.h>
#include <util/xml_node.h>

#include <driver.h>
#include <lx_emul.h>

#include <legacy/lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <legacy/lx_emul/extern_c_end.h>

#define TRACE do { ; } while (0)

#include <legacy/lx_emul/impl/kernel.h>
#include <legacy/lx_emul/impl/delay.h>
#include <legacy/lx_emul/impl/slab.h>
#include <legacy/lx_emul/impl/work.h>
#include <legacy/lx_emul/impl/spinlock.h>
#include <legacy/lx_emul/impl/mutex.h>
#include <legacy/lx_emul/impl/sched.h>
#include <legacy/lx_emul/impl/timer.h>
#include <legacy/lx_emul/impl/completion.h>
#include <legacy/lx_emul/impl/wait.h>
#include <legacy/lx_emul/impl/usb.h>

#include <legacy/lx_kit/backend_alloc.h>

#include <legacy/lx_emul/extern_c_begin.h>

#include <linux/mii.h>
#include <linux/usb/usbnet.h>


static int usb_match_device(struct usb_device *dev,
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


static int usb_match_one_id_intf(struct usb_device *dev,
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


int usb_match_one_id(struct usb_interface *interface,
                     const struct usb_device_id *id)
{
	struct usb_host_interface *intf;
	struct usb_device *dev;

	if (id == NULL)
		return 0;

	intf = interface->cur_altsetting;
	dev = interface_to_usbdev(interface);

	if (!usb_match_device(dev, id))
		return 0;

	return usb_match_one_id_intf(dev, intf, id);
}

#include <legacy/lx_emul/extern_c_end.h>


class Addr_to_page_mapping : public Genode::List<Addr_to_page_mapping>::Element
{
	private:

		struct page  *_page { nullptr };

		static Genode::List<Addr_to_page_mapping> & _list()
		{
			static Genode::List<Addr_to_page_mapping> _l;
			return _l;
		}

	public:

		Addr_to_page_mapping(struct page *page)
			: _page(page) { }

		static void insert(struct page * page)
		{
			Addr_to_page_mapping *m = (Addr_to_page_mapping*)
				Lx::Malloc::mem().alloc(sizeof (Addr_to_page_mapping));
			m->_page = page;
			_list().insert(m);
		}

		static struct page * remove(unsigned long addr)
		{
			for (Addr_to_page_mapping *m = _list().first(); m; m = m->next())
				if ((unsigned long)m->_page->addr == addr) {
					struct page * ret = m->_page;
					_list().remove(m);
					Lx::Malloc::mem().free(m);
					return ret;
				}

			return nullptr;
		}
};

struct Lx_driver
{
	using Element = Genode::List_element<Lx_driver>;
	using List    = Genode::List<Element>;

	usb_driver & drv;
	Element      le { this };

	Lx_driver(usb_driver & drv) : drv(drv) { list().insert(&le); }

	usb_device_id * match(usb_interface * iface)
	{
		struct usb_device_id * id = const_cast<usb_device_id*>(drv.id_table);
		for (; id->idVendor || id->idProduct || id->bDeviceClass ||
		       id->bInterfaceClass || id->driver_info; id++)
			if (usb_match_one_id(iface, id)) return id;
		return nullptr;
	}

	int probe(usb_interface * iface, usb_device_id * id)
	{
		iface->dev.driver = &drv.drvwrap.driver;
		if (drv.probe) return drv.probe(iface, id);
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


int usb_register_driver(struct usb_driver * driver, struct module *, const char *)
{
	INIT_LIST_HEAD(&driver->dynids.list);
	if (driver) new (Lx::Malloc::mem()) Lx_driver(*driver);
	return 0;
}


Genode::addr_t Lx::backend_dma_addr(Genode::Ram_dataspace_capability)
{
	return 0;
}


int usb_driver_claim_interface(struct usb_driver *driver, struct usb_interface *iface, void *priv)
{
	usb_device      *udev = interface_to_usbdev(iface);
	Usb::Connection *usb = reinterpret_cast<Usb::Connection *>(udev->bus->controller);
	try {
		usb->claim_interface(iface->cur_altsetting->desc.bInterfaceNumber);
	} catch (...) {
		return -1;
	}
	return 0;
}


int usb_set_interface(struct usb_device *udev, int ifnum, int alternate)
{
	Usb::Connection *usb = reinterpret_cast<Usb::Connection *>(udev->bus->controller);
	Driver::Sync_packet packet { *usb };
	packet.alt_setting(ifnum, alternate);
	usb_interface *iface = udev->config->interface[ifnum];
	iface->cur_altsetting = &iface->altsetting[alternate];

	return 0;
}

void Driver::Device::probe_interface(usb_interface * iface, usb_device_id * id)
{
	using Le = Genode::List_element<Lx_driver>;
	for (Le *le = Lx_driver::list().first(); le; le = le->next()) {
		usb_device_id * id = le->object()->match(iface);

		if (id) {
			int ret = le->object()->probe(iface, id);
			if (ret == 0) return;
		}
	}
}


void Driver::Device::remove_interface(usb_interface * iface)
{
	/* we might not drive this interface */
	if (iface->dev.driver) {
		usbnet *dev =(usbnet* )usb_get_intfdata(iface);
		usbnet_link_change(dev, 0, 0);
		to_usb_driver(iface->dev.driver)->disconnect(iface);
	}

	for (unsigned i = 0; i < iface->num_altsetting; i++) {
		if (iface->altsetting[i].extra)
			kfree(iface->altsetting[i].extra);
		kfree(iface->altsetting[i].endpoint);
	}

	kfree(iface->altsetting);
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


u16 get_unaligned_le16(const void *p)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *)p;
	return ptr->x;
}


u32 get_unaligned_le32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}


enum { MAC_LEN = 17 };


static void snprint_mac(char *buf, u8 *mac)
{
	for (int i = 0; i < ETH_ALEN; i++) {
		Format::snprintf((char *)&buf[i * 3], 3, "%02x", mac[i]);
		if ((i * 3) < MAC_LEN)
			buf[(i * 3) + 2] = ':';
	}

	buf[MAC_LEN] = 0;
}


static void random_ether_addr(u8 *addr)
{
	using namespace Genode;

	char str[MAC_LEN + 1];
	u8 fallback[] = { 0x2e, 0x60, 0x90, 0x0c, 0x4e, 0x01 };
	Nic::Mac_address mac;

	Xml_node config_node = Lx_kit::env().config_rom().xml();

	/* try using configured mac */
	try {
		Xml_node::Attribute mac_node = config_node.attribute("mac");
		mac_node.value(mac);
	} catch (...) {
		/* use fallback mac */
		snprint_mac(str, fallback);
		Genode::warning("No mac address or wrong format attribute in <nic> - using fallback (", Genode::Cstring(str), ")");

		Genode::memcpy(addr, fallback, ETH_ALEN);
		return;
	}

	/* use configured mac*/
	Genode::memcpy(addr, mac.addr, ETH_ALEN);
	snprint_mac(str, (u8 *)mac.addr);
	Genode::log("Using configured mac: ", Genode::Cstring(str));
}


void eth_hw_addr_random(struct net_device *dev)
{
	random_ether_addr(dev->dev_addr);
}


void eth_random_addr(u8 *addr)
{
	random_ether_addr(addr);
}


struct net_device *alloc_etherdev(int sizeof_priv)
{
	net_device *dev      = (net_device*) kzalloc(sizeof(net_device), 0);
	dev->mtu             = 1500;
	dev->hard_header_len = 0;
	dev->priv            = kzalloc(sizeof_priv, 0);
	dev->dev_addr        = (unsigned char*) kzalloc(ETH_ALEN, 0);
	//memset(dev->_dev_addr, 0, sizeof(dev->_dev_addr));
	return dev;
}


void free_netdev(struct net_device * ndev)
{
	if (!ndev) return;

	kfree(ndev->priv);
	kfree(ndev->dev_addr);
	kfree(ndev);
}


void *__alloc_percpu(size_t size, size_t align)
{
	return kmalloc(size, 0);
}


int mii_nway_restart(struct mii_if_info *mii)
{
	/* if autoneg is off, it's an error */
	int bmcr = mii->mdio_read(mii->dev, mii->phy_id, MII_BMCR);

	if (bmcr & BMCR_ANENABLE) {
		printk("Reenable\n");
		bmcr |= BMCR_ANRESTART;
		mii->mdio_write(mii->dev, mii->phy_id, MII_BMCR, bmcr);
		return 0;
	}

	return -EINVAL;
}


static net_device * single_net_device = nullptr;

int register_netdev(struct net_device *dev)
{
	dev->state |= 1 << __LINK_STATE_START;

	int err = dev->netdev_ops->ndo_open(dev);

	if (err) return err;

	if (dev->netdev_ops->ndo_set_rx_mode)
		dev->netdev_ops->ndo_set_rx_mode(dev);
	single_net_device = dev;
	return 0;
};


void unregister_netdev(struct net_device * dev)
{
	if (dev->netdev_ops->ndo_stop)
		dev->netdev_ops->ndo_stop(dev);

	single_net_device = NULL;
}


net_device *
Linux_network_session_base::
_register_session(Linux_network_session_base &session,
                  Genode::Session_label       policy)
{
	if (single_net_device) single_net_device->session_component = &session;
	return single_net_device;
}


void tasklet_schedule(struct tasklet_struct *t)
{
	Lx::Work *lx_work = (Lx::Work *)tasklet_wq->task;
	lx_work->schedule_tasklet(t);
	lx_work->unblock();
}


struct workqueue_struct *create_singlethread_workqueue(char const *name)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	Lx::Work *work = Lx::Work::alloc_work_queue(&Lx::Malloc::mem(), name);
	wq->task       = (void *)work;

	return wq;
}


struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags,
                                         int max_active, ...)
{
	return create_singlethread_workqueue(fmt);
}


int dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
	return 0;
}


void * dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}


int netif_running(const struct net_device *dev)
{
	return dev->state & (1 << __LINK_STATE_START);
}


void netif_carrier_off(struct net_device *dev)
{
	dev->state |= 1 << __LINK_STATE_NOCARRIER;
	if (dev->session_component)
		reinterpret_cast<Linux_network_session_base*>(dev->session_component)->
			link_state(false);
}


int netif_carrier_ok(const struct net_device *dev)
{
	return !(dev->state & (1 << __LINK_STATE_NOCARRIER));
}


void *kmem_cache_alloc_node(struct kmem_cache *cache, gfp_t gfp_flags, int arg)
{
	return (void*)cache->alloc_element();
}


int mii_ethtool_gset(struct mii_if_info *mii, struct ethtool_cmd *ecmd)
{
	ecmd->duplex = DUPLEX_FULL;
	return 0;
}


void netif_carrier_on(struct net_device *dev)
{
	dev->state &= ~(1 << __LINK_STATE_NOCARRIER);
	if (dev->session_component)
		reinterpret_cast<Linux_network_session_base*>(dev->session_component)->
			link_state(true);
}


int netif_rx(struct sk_buff * skb)
{
	if (skb->dev->session_component)
		reinterpret_cast<Linux_network_session_base*>(skb->dev->session_component)->
			receive(skb);

	dev_kfree_skb(skb);
	return NET_RX_SUCCESS;
}


void dev_kfree_skb_any(struct sk_buff *skb)
{
	dev_kfree_skb(skb);
}

int is_valid_ether_addr(const u8 * a)
{
	for (unsigned i = 0; i < ETH_ALEN; i++)
		if (a[i] != 0 && a[i] != 255) return 1;
	return 0;
}


unsigned int mii_check_media (struct mii_if_info *mii, unsigned int ok_to_print, unsigned int init_media)
{
	if (mii_link_ok(mii)) netif_carrier_on(mii->dev);
	else                  netif_carrier_off(mii->dev);
	return 0;
}


int mii_link_ok (struct mii_if_info *mii)
{
	/* first, a dummy read, needed to latch some MII phys */
	mii->mdio_read(mii->dev, mii->phy_id, MII_BMSR);
	if (mii->mdio_read(mii->dev, mii->phy_id, MII_BMSR) & BMSR_LSTATUS)
		return 1;
	return 0;
}


static struct page *allocate_pages(gfp_t gfp_mask, unsigned int size)
{
	struct page *page = (struct page *)kzalloc(sizeof(struct page), 0);

	page->addr = Lx::Malloc::dma().alloc_large(size);
	page->size = size;

	if (!page->addr) {
		Genode::error("alloc_pages: ", size, " failed");
		kfree(page);
		return 0;
	}

	Addr_to_page_mapping::insert(page);

	atomic_set(&page->_count, 1);

	return page;
}


void *page_frag_alloc(struct page_frag_cache *nc, unsigned int fragsz, gfp_t gfp_mask)
{
	struct page *page = allocate_pages(gfp_mask, fragsz);
	if (!page) return nullptr;

	return page->addr;
}


void page_frag_free(void *addr)
{
	struct page *page = Addr_to_page_mapping::remove((unsigned long)addr);

	if (!atomic_dec_and_test(&page->_count))
		Genode::error("page reference count != 0");

	Lx::Malloc::dma().free_large(page->addr);
	kfree(page);
}


