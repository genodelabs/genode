/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Stefan Kalkowski
 * \date   2018-01-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/**
 * Unconditionally include common Genode headers _before_ lx_emul.h to
 * prevent shenanigans with macro definitions.
 */
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/snprintf.h>
#include <gpio_session/connection.h>
#include <irq_session/client.h>
#include <os/backtrace.h>

#include <component.h>
#include <lx_emul.h>

#include <lx_emul/impl/kernel.h>
#include <lx_emul/impl/delay.h>
#include <lx_emul/impl/slab.h>
#include <lx_emul/impl/work.h>
#include <lx_emul/impl/spinlock.h>
#include <lx_emul/impl/mutex.h>
#include <lx_emul/impl/sched.h>
#include <lx_emul/impl/timer.h>
#include <lx_emul/impl/completion.h>
#include <lx_kit/irq.h>

extern "C" { struct page; }

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

		static struct page * find_page(void* addr)
		{
			for (Addr_to_page_mapping *m = _list().first(); m; m = m->next())
				if ((unsigned long)m->_page->addr <= (unsigned long)addr &&
					((unsigned long)m->_page->addr + m->_page->size) > (unsigned long)addr)
					return m->_page;
			return nullptr;
		}
};


struct Device : Genode::List<Device>::Element
{
	struct device * dev; /* Linux device */

	Device(struct device *dev) : dev(dev) {
		list()->insert(this); }

	static Genode::List<Device> *list()
	{
		static Genode::List<Device> _list;
		return &_list;
	}
};


class Driver : public Genode::List<Driver>::Element
{
	private:

		struct device_driver * _drv; /* Linux driver */

	public:

		Driver(struct device_driver *drv) : _drv(drv)
		{
			list()->insert(this);
		}

		/**
		 * List of all currently registered drivers
		 */
		static Genode::List<Driver> *list()
		{
			static Genode::List<Driver> _list;
			return &_list;
		}

		/**
		 * Match device and drivers
		 */
		bool match(struct device *dev)
		{
			/*
			 *  Don't try if buses don't match, since drivers often use 'container_of'
			 *  which might cast the device to non-matching type
			 */
			if (_drv->bus != dev->bus)
				return false;

			return _drv->bus->match ? _drv->bus->match(dev, _drv) : true;
		}

		/**
		 * Probe device with driver
		 */
		int probe(struct device *dev)
		{
			dev->driver = _drv;

			if (dev->bus->probe) return dev->bus->probe(dev);
			else if (_drv->probe)
				return _drv->probe(dev);

			return 0;
		}
};


struct Gpio_irq : public Genode::List<Gpio_irq>::Element
{
	unsigned                         irq_nr;
	bool                             enabled = true;
	bool                             pending = false;
	Gpio::Connection                 gpio;
	Genode::Irq_session_client       irq;
	Genode::Signal_handler<Gpio_irq> sigh;
	Lx::Task                         task;
	irq_handler_t                    ihandler;
	void *                           dev_id;

	/**
	 * List of all currently registered irqs
	 */
	static Genode::List<Gpio_irq> *list()
	{
		static Genode::List<Gpio_irq> _list;
		return &_list;
	}

	static void run_irq(void *args)
	{
		Gpio_irq * girq = static_cast<Gpio_irq*>(args);
		while (1) {
			Lx::scheduler().current()->block_and_schedule();
			girq->ihandler(girq->irq_nr, girq->dev_id);
			girq->irq.ack_irq();
		}
	}

	void unblock()
	{
		if (enabled) task.unblock();

		pending = !enabled;
	}

	void enable()
	{
		enabled = true;
		if (pending) unblock();
	}

	void disable()
	{
		enabled = false;
	}

	Gpio_irq(Genode::Env &env, unsigned nr, irq_handler_t handler, void * dev_id)
	: irq_nr(nr),
	  gpio(env, nr),
	  irq(gpio.irq_session(Gpio::Session::LOW_LEVEL)),
	  sigh(env.ep(), *this, &Gpio_irq::unblock),
	  task(run_irq, this, "gpio_irq", Lx::Task::PRIORITY_3, Lx::scheduler()),
	  ihandler(handler),
	  dev_id(dev_id)
	{
		list()->insert(this);

		irq.sigh(sigh);
		irq.ack_irq();
	}
};


struct Fec
{
	using String = Genode::String<128>;

	struct Mdio
	{
		struct Phy
		{
			String              name;
			String              phy_driver;
			const unsigned      phy_reg;
			const unsigned      gpio_irq;
			struct phy_device * phy_dev { nullptr };

			Phy(String name, String driver, const unsigned reg, const unsigned irq)
			: name(name), phy_driver(driver), phy_reg(reg), gpio_irq(irq) {}
		};

		enum { MAX = 10 };
		Genode::Constructible<Phy> phys[MAX];

		template <typename FUNC>
		void for_each(FUNC && f)
		{
			for (unsigned i = 0; i < MAX; i++)
				if (phys[i].constructed()) f(*phys[i]);
		}
	};

	String                            name;
	String                            device;
	const unsigned                    irq;
	const size_t                      mmio;
	String                            phy_mode;
	const bool                        magic_packet;
	const int                         tx_queues;
	const int                         rx_queues;
	struct net_device *               net_dev { nullptr };
	Session_component *               session { nullptr };
	Genode::Attached_io_mem_dataspace io_ds   { Lx_kit::env().env(), mmio, 0x4000 };
	Genode::Constructible<Mdio>       mdio;
	Mdio::Phy *                       phy { nullptr };

	Fec(String name, String device, const unsigned irq,
	    const size_t mmio, String mode, const bool magic = true,
		const int tx_queues = 1, const int rx_queues = 1)
	: name(name), device(device), irq(irq), mmio(mmio), phy_mode(mode), magic_packet(magic),
	  tx_queues(tx_queues), rx_queues(rx_queues) {};
};


static const unsigned FEC_MAX = 2;
static Genode::Constructible<Fec> fec_devices[FEC_MAX];

net_device * Session_component::_register_session_component(Session_component & s,
                                                            Genode::Session_label policy)
{
	Genode::Session_label name = policy.last_element();

	for (unsigned i = 0; i < FEC_MAX; i++) {

		/* No more cards available */
		if (!fec_devices[i].constructed()) return nullptr;

		/* Session does not match cards policy */
		if (fec_devices[i]->name.length() > 1 &&
		    fec_devices[i]->name != name) continue;

		/* Session already in use */
		if (fec_devices[i]->session) return nullptr;

		fec_devices[i]->session = &s;
		return fec_devices[i]->net_dev;
	}
	return nullptr;
}


#include <lx_emul/extern_c_begin.h>
#include <linux/phy.h>
#include <linux/timecounter.h>
#include <lx_emul/extern_c_end.h>

extern "C" {

void lx_backtrace() { Genode::backtrace(); }

int platform_driver_register(struct platform_driver * drv)
{
	using String = Fec::String;

	try {
		unsigned i = 0;
		Genode::Attached_rom_dataspace config { Lx_kit::env().env(), "config" };
		config.xml().for_each_sub_node("card", [&] (Genode::Xml_node const node) {
			if (i == FEC_MAX) {
				Genode::error("More cards defined than available!");
				return;
			}

			String name         = node.attribute_value("name", String());
			String type         = node.attribute_value("type", String());
			String mdio         = node.attribute_value("mii",  String());
			String phy          = node.attribute_value("phy",  String());
			unsigned irq        = node.attribute_value("irq",  0UL);
			Genode::addr_t mmio = node.attribute_value("mmio", 0UL);
			bool   magic        = node.attribute_value("magic_packet", true);
			unsigned txq        = node.attribute_value("tx-queues", 1UL);
			unsigned rxq        = node.attribute_value("rx-queues", 1UL);
			fec_devices[i].construct(name, type, irq, mmio, mdio, magic, txq, rxq);

			node.for_each_sub_node("mdio", [&] (Genode::Xml_node const node) {
				fec_devices[i]->mdio.construct();
				unsigned j = 0;
				node.for_each_sub_node("phy", [&] (Genode::Xml_node const node) {
					String name  = node.attribute_value("name", String());
					String type  = node.attribute_value("type", String());
					unsigned irq = node.attribute_value("gpio_irq", 0UL);
					unsigned reg = node.attribute_value("reg_num",  0UL);
					fec_devices[i]->mdio->phys[j].construct(name, type, reg, irq);
					j++;
				});
			});

			for (unsigned k = 0; k <= i; k++)
				if (fec_devices[k]->mdio.constructed()) {
					fec_devices[k]->mdio->for_each([&] (Fec::Mdio::Phy & p) {
						if (p.name == phy) fec_devices[i]->phy = &p; });
				}

			i++;
		});
	} catch(...) { }

	if (!fec_devices[0].constructed()) {
		Genode::warning("No valid configuration provided, use default values");
		fec_devices[0].construct(String(), "fsl,imx6q-fec", 150, 0x2188000, "rgmii");
	}

	for (unsigned i = 0; i < FEC_MAX; i++) {
		if (!fec_devices[i].constructed()) break;

		platform_device * pd = new (Lx::Malloc::dma()) platform_device();
		pd->name         = fec_devices[i]->name.string();
		pd->dev.of_node  = (device_node*) &fec_devices[i];
		pd->dev.plat_dev = pd;
		drv->probe(pd);
		{
			net_device * dev = fec_devices[i]->net_dev;
			int err = dev ? dev->netdev_ops->ndo_open(dev) : -1;
			if (err) {
				Genode::error("ndo_open()  failed: ", err);
				return err;
			}
		}
	}
	return 0;
}


struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name,
                                    unsigned char name_assign_type,
                                    void (*setup)(struct net_device *),
                                    unsigned int txqs, unsigned int rxqs)
{
	size_t alloc_size = ALIGN(sizeof(struct net_device), NETDEV_ALIGN);
	alloc_size += sizeof_priv;
	alloc_size += NETDEV_ALIGN - 1;

	struct net_device *p   = (struct net_device*) kzalloc(alloc_size,
	                                                      GFP_KERNEL);
	struct net_device *dev = PTR_ALIGN(p, NETDEV_ALIGN);

	INIT_LIST_HEAD(&dev->mc.list);
	dev->mc.count = 0;

	dev->gso_max_segs = GSO_MAX_SEGS;

	setup(dev);

	static const struct ethtool_ops default_ethtool_ops { };
	if (!dev->ethtool_ops) dev->ethtool_ops = &default_ethtool_ops;

	dev->dev_addr = (unsigned char*) kzalloc(ETH_ALEN, GFP_KERNEL);

	return dev;
}


bool of_device_is_available(const struct device_node *device)
{
	return device;
}


const struct of_device_id *of_match_device(const struct of_device_id *matches,
                                           const struct device *dev)
{
	Fec * fec = (Fec*) dev->plat_dev->dev.of_node;
	for (; matches && matches->compatible[0]; matches++)
		if (Genode::strcmp(matches->compatible, fec->device.string()) == 0)
			return matches;

	return nullptr;
}

void * devm_ioremap_resource(struct device *dev, struct resource *res)
{
	Fec * fec = (Fec*) dev->plat_dev->dev.of_node;

	return fec->io_ds.local_addr<void>();
}


void platform_set_drvdata(struct platform_device *pdev, void *data)
{
	pdev->dev.driver_data = data;
	struct net_device * ndev = (net_device*)data;
	ndev->dev.of_node = pdev->dev.of_node;
}

int of_get_phy_mode(struct device_node *np)
{
	Fec * fec = (Fec*) np;

	for (int i = 0; i < PHY_INTERFACE_MODE_MAX; i++)
		if (!Genode::strcmp(fec->phy_mode.string(),
		                    phy_modes((phy_interface_t)i)))
			return i;

	return -ENODEV;
}


ktime_t ktime_get_real(void)
{
	Lx::timer_update_jiffies();
	return ktime_get();
}


void timecounter_init(struct timecounter *tc, const struct cyclecounter *cc, u64 start_tstamp)
{
	tc->cc = cc;
	tc->cycle_last = cc->read(cc);
	tc->nsec = start_tstamp;
	tc->mask = (1ULL << cc->shift) - 1;
	tc->frac = 0;
}


void *dma_alloc_coherent(struct device *dev, size_t size,
                         dma_addr_t *dma_handle, gfp_t flag)
{
	void *addr = Lx::Malloc::dma().alloc_large(size);
	dma_addr_t dma_addr = (dma_addr_t) Lx::Malloc::dma().phys_addr(addr);

	*dma_handle = (dma_addr_t) dma_addr;
	return addr;
}


void *dmam_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp)
{
	dma_addr_t dma_addr;
	void *addr;
	if (size > 2048) {
		addr = Lx::Malloc::dma().alloc_large(size);
		dma_addr = (dma_addr_t) Lx::Malloc::dma().phys_addr(addr);
	} else
		addr = Lx::Malloc::dma().alloc(size, 12, &dma_addr);

	*dma_handle = dma_addr;
	return addr;
}


dma_addr_t dma_map_single(struct device *dev, void *cpu_addr, size_t size,
                          enum dma_data_direction)
{
	dma_addr_t dma_addr = (dma_addr_t) Lx::Malloc::dma().phys_addr(cpu_addr);
	
	if (dma_addr == ~0UL) {

		struct page * p = Addr_to_page_mapping::find_page(cpu_addr);
		if (p) {
			dma_addr = (dma_addr_t) Lx::Malloc::dma().phys_addr(p->addr);
			dma_addr += (dma_addr_t)cpu_addr - (dma_addr_t)p->addr;
		}

		if (dma_addr == ~0UL)
			Genode::error(__func__, ": virtual address ", cpu_addr,
			              " not registered for DMA");
	}

	return dma_addr;
}


int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return (dma_addr == ~0UL) ? 1 : 0;
}


void *dev_get_platdata(const struct device *dev)
{
	return dev->platform_data;
}


int netif_running(const struct net_device *dev)
{
	return dev->state & (1 << __LINK_STATE_START);
}


void netif_carrier_on(struct net_device *dev)
{
	dev->state &= ~(1UL << __LINK_STATE_NOCARRIER);

	Fec * fec = (Fec*) dev->dev.of_node;
	if (fec->session) fec->session->link_state(true);
}


void netif_carrier_off(struct net_device *dev)
{
	dev->state |= 1UL << __LINK_STATE_NOCARRIER;

	Fec * fec = (Fec*) dev->dev.of_node;
	if (fec->session) fec->session->link_state(false);
}


int netif_device_present(struct net_device * d)
{
	TRACE;
	return 1;
}



int platform_get_irq(struct platform_device * d, unsigned int i)
{
	if (i > 1) return -1;

	Fec * fec = (Fec*) d->dev.of_node;

	return fec->irq + i;
}


int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id)
{
	Lx::Irq::irq().request_irq(Platform::Device::create(Lx_kit::env().env(), irq), handler, dev_id);
	return 0;
}


struct clk *devm_clk_get(struct device *dev, const char *id)
{
	static struct clk clocks[] {
		{ "ipg", 66*1000*1000 },
		{ "ahb", 198*1000*1000 },
		{ "ptp", 25*1000*1000 },
		{ "enet_out", 25*1000*1000 },
		{ "enet_clk_ref", 125*1000*1000 } };

	for (unsigned i = 0; i < (sizeof(clocks) / sizeof(struct clk)); i++)
		if (Genode::strcmp(clocks[i].name, id) == 0)
			return &clocks[i];

	Genode::error("MISSING CLOCK: ", id);
	return nullptr;
}


unsigned long clk_get_rate(struct clk * clk)
{
	if (!clk) return 0;
	return clk->rate;
}


int is_valid_ether_addr(const u8 * a)
{
	for (unsigned i = 0; i < ETH_ALEN; i++)
		if (a[i] != 0 && a[i] != 255) return 1;
	return 0;
}


int register_netdev(struct net_device * d)
{
	d->state |= (1 << __LINK_STATE_START) | (1UL << __LINK_STATE_NOCARRIER);

	Fec * fec = (Fec*) d->dev.of_node;
	fec->net_dev = d;

	return 0;
}


void *kmem_cache_alloc_node(struct kmem_cache *cache, gfp_t, int)
{
	return (void*)cache->alloc();
}


void get_page(struct page *page)
{
	atomic_inc(&page->_count);
}


static struct page *allocate_pages(gfp_t gfp_mask, size_t const size)
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


struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	size_t const size = PAGE_SIZE << order;
	return allocate_pages(gfp_mask, size);
}


void *__alloc_page_frag(struct page_frag_cache *, unsigned int const fragsz,
                        gfp_t const gfp_mask)
{
	struct page *page = allocate_pages(gfp_mask, fragsz);
	return page ? page->addr : page;
}


void __free_page_frag(void *addr)
{
	struct page *page = Addr_to_page_mapping::remove((unsigned long)addr);

	if (!atomic_dec_and_test(&page->_count))
		Genode::error("page reference count != 0");

	Lx::Malloc::dma().free_large(page->addr);
	kfree(page);
}

int driver_register(struct device_driver *drv)
{
	new (Lx::Malloc::mem()) Driver(drv);
	return 0;
}


int device_add(struct device *dev)
{
	if (dev->driver)
		return 0;

	/* foreach driver match and probe device */
	for (Driver *driver = Driver::list()->first(); driver; driver = driver->next())
		if (driver->match(dev)) {
			int ret = driver->probe(dev);

			if (!ret) return 0;
		}

	return 0;
}


void device_del(struct device *dev)
{
	if (dev->driver && dev->driver->remove)
		dev->driver->remove(dev);
}


int device_register(struct device *dev)
{
	return device_add(dev);
}


void reinit_completion(struct completion *work)
{
	init_completion(work);
}


static void _completion_timeout(struct timer_list *t)
{
	Lx::Task *task = (Lx::Task *)t->data;
	task->unblock();
}


long __wait_completion(struct completion *work, unsigned long timeout)
{
	timer_list t;
	Lx::timer_update_jiffies();
	unsigned long j = timeout ? jiffies + timeout : 0;

	if (timeout) {
		timer_setup(&t, _completion_timeout, 0u);
		t.data = (unsigned long) Lx::scheduler().current();
		mod_timer(&t, j);
	}

	while (!work->done) {

		if (j && j <= jiffies) {
			lx_log(1, "timeout jiffies %lu", jiffies);
			return 0;
		}

		Lx::Task *task = Lx::scheduler().current();
		work->task = (void *)task;
		task->block_and_schedule();
	}

	if (timeout)
		del_timer(&t);

	work->done = 0;

	return (j  || j == jiffies) ? 1 : j - jiffies;
}


size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = Genode::strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		Genode::memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}


void local_irq_restore(unsigned long f) { }


unsigned long local_irq_save(unsigned long flags) { return flags; }


int pm_runtime_get_sync(struct device *dev)
{
	return 0;
}


void pm_runtime_mark_last_busy(struct device *dev) { }


int in_interrupt(void)
{
	return 0;
}


int pm_runtime_put_autosuspend(struct device *dev)
{
	return 0;
}


int dev_set_name(struct device *dev, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	Genode::String_console sc(dev->name, 32);
	sc.vprintf(fmt, args);

	va_end(args);

	new (Lx::Malloc::mem()) Device(dev);
	return 0;
}


struct device *bus_find_device_by_name(struct bus_type *bus, struct device *start, const char *name)
{
	for (Device *dev = Device::list()->first(); dev; dev = dev->next()) {
		if (Genode::strcmp(dev->dev->name, name) == 0)
			return dev->dev;
	}
	return nullptr;
}


void netif_napi_add(struct net_device *dev, struct napi_struct *napi,
                    int (*poll)(struct napi_struct *, int), int weight)
{
	napi->dev    = dev;
	napi->poll   = poll;
	napi->state  = NAPI_STATE_SCHED;
	napi->weight = weight;
}


const char *dev_name(const struct device *dev)
{
	return dev->name;
}


extern "C" void consume_skb(struct sk_buff *skb);

void dev_kfree_skb_any(struct sk_buff * sk)
{
	consume_skb(sk);
}


void napi_enable(struct napi_struct *n)
{
	clear_bit(NAPI_STATE_SCHED, &n->state);
	clear_bit(NAPI_STATE_NPSVC, &n->state);
}


void napi_disable(struct napi_struct *n)
{
	set_bit(NAPI_STATE_SCHED, &n->state);
	set_bit(NAPI_STATE_NPSVC, &n->state);
}


void __napi_schedule(struct napi_struct *n)
{
	Fec * fec = (Fec*) n->dev->dev.of_node;
	if (fec->session) fec->session->unblock_rx_task(n);
}


bool napi_schedule_prep(struct napi_struct *n)
{
	return !test_and_set_bit(NAPI_STATE_SCHED, &n->state);
}


void napi_complete(struct napi_struct *n)
{
	clear_bit(NAPI_STATE_SCHED, &n->state);
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


gro_result_t napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb)
{
	Fec * fec = (Fec*) napi->dev->dev.of_node;
	if (fec->session) fec->session->receive(skb);

	dev_kfree_skb(skb);
	return GRO_NORMAL;
}


void dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	// FIXME
	TRACE;
}


bool netif_queue_stopped(const struct net_device *dev)
{
	// FIXME
	TRACE;
	return 0;
}


struct device_node *of_parse_phandle(const struct device_node *np, const char *phandle_name, int index)
{
	Fec * fec = (Fec*) np;
	return (device_node*) fec->phy;
}


struct phy_device *of_phy_connect(struct net_device *dev,
                                  struct device_node *phy_np,
                                  void (*hndlr)(struct net_device *),
                                  u32 flags, int iface)
{
	Fec::Mdio::Phy * phy = (Fec::Mdio::Phy*) phy_np;
	struct phy_device * phydev = phy ? phy->phy_dev : nullptr;
	if (!phydev) return nullptr;

	phydev->dev_flags = flags;
	int ret = phy_connect_direct(dev, phydev, hndlr, (phy_interface_t)iface);
	return ret ? nullptr : phydev;
}


struct device_node *of_get_child_by_name(const struct device_node *node,
                                         const char *name)
{
	if (Genode::strcmp("mdio", name) != 0) return nullptr;

	Fec * fec = (Fec*) node;
	return fec->mdio.constructed() ? (device_node*) &*fec->mdio : nullptr;
}


static int of_mdiobus_register_phy(Fec::Mdio::Phy & ph, struct mii_bus *mdio)
{
	struct phy_device * phy = get_phy_device(mdio, ph.phy_reg, false);

	if (!phy || IS_ERR(phy)) return 1;

	phy->irq         = ph.gpio_irq;
	phy->dev.of_node = (device_node*) &ph;

	/* All data is now stored in the phy struct;
	 * register it */
	int rc = phy_device_register(phy);
	if (rc) {
		phy_device_free(phy);
		return 1;
	}

	ph.phy_dev = phy;

	dev_dbg(&mdio->dev, "registered phy at address %i\n", ph.phy_reg);

	return 0;
}

int of_mdiobus_register(struct mii_bus *mdio, struct device_node *np)
{
	Fec::Mdio * fec_m = (Fec::Mdio*) np;

	mdio->phy_mask = ~0;

	/* Clear all the IRQ properties */
	if (mdio->irq)
		for (unsigned i = 0; i<PHY_MAX_ADDR; i++)
			mdio->irq[i] = PHY_POLL;

	mdio->dev.of_node = np;

	/* Register the MDIO bus */
	int rc = mdiobus_register(mdio);
	if (rc) return rc;

	fec_m->for_each([&] (Fec::Mdio::Phy & phy) {
					of_mdiobus_register_phy(phy, mdio); });
	return 0;
}


int of_driver_match_device(struct device *dev, const struct device_driver *drv)
{
	Fec::Mdio::Phy * phy = (Fec::Mdio::Phy*) dev->of_node;

	return phy ? (Genode::strcmp(drv->name,
	                             phy->phy_driver.string()) == 0) : 0;
}


const void *of_get_property(const struct device_node *node, const char *name, int *lenp)
{
	Fec * fec = (Fec*) node;
	if (Genode::strcmp("fsl,magic-packet", name) == 0) return (void*)fec->magic_packet;

	TRACE_AND_STOP;
	return nullptr;
}


int of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value)
{
	Fec * fec = (Fec*) np;

	if (Genode::strcmp("max-speed", propname) == 0) return 1;

	if ((Genode::strcmp("fsl,num-tx-queues", propname) == 0) && fec->tx_queues)
		*out_value = fec->tx_queues;
	else if ((Genode::strcmp("fsl,num-rx-queues", propname) == 0) && fec->rx_queues)
		*out_value = fec->rx_queues;
	else
		TRACE_AND_STOP;

	return 0;
}


void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	if (size > 2048) Genode::warning("devm_kzalloc ", size);
	return Lx::Malloc::mem().alloc(size);
}


int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev)
{
	new (Lx::Malloc::mem()) Gpio_irq(Lx_kit::env().env(), irq, handler, dev);
	return 0;
}


int enable_irq(unsigned int irq)
{
	for (Gpio_irq *girq = Gpio_irq::list()->first(); girq; girq = girq->next())
		if (girq->irq_nr == irq) {
			girq->enable();
			return 0;
		}
	return 1;
}


int disable_irq_nosync(unsigned int irq)
{
	for (Gpio_irq *girq = Gpio_irq::list()->first(); girq; girq = girq->next())
		if (girq->irq_nr == irq) {
			girq->disable();
			return 0;
		}
	return 1;
}


struct device_node *of_get_next_available_child(const struct device_node *node, struct device_node *prev)
{
	TRACE_AND_STOP;
	return nullptr;
}


u64 timecounter_read(struct timecounter *tc)
{
	u64 nsec;

	cycle_t cycle_now, cycle_delta;

	/* increment time by nanoseconds since last call */
	{
		/* read cycle counter: */
		cycle_now = tc->cc->read(tc->cc);

		/* calculate the delta since the last timecounter_read_delta(): */
		cycle_delta = (cycle_now - tc->cycle_last) & tc->cc->mask;

		/* convert to nanoseconds: */
		nsec = cyclecounter_cyc2ns(tc->cc, cycle_delta,
								   tc->mask, &tc->frac);

		/* update time stamp of timecounter_read_delta() call: */
		tc->cycle_last = cycle_now;
	}

	nsec += tc->nsec;
	tc->nsec = nsec;

	return nsec;
}

/*********************
 ** DUMMY FUNCTIONS **
 *********************/

void clk_disable_unprepare(struct clk * c)
{
	TRACE;
}

int clk_prepare_enable(struct clk * c)
{
	TRACE;
	return 0;
}

void device_initialize(struct device *dev)
{
	TRACE;
}

int device_init_wakeup(struct device *dev, bool val)
{
	TRACE;
	return 0;
}

struct regulator *__must_check devm_regulator_get(struct device *dev, const char *id)
{
	TRACE;
	return nullptr;
}

struct netdev_queue *netdev_get_tx_queue(const struct net_device *dev, unsigned int index)
{
	TRACE;
	return nullptr;
}

bool of_phy_is_fixed_link(struct device_node *np)
{
	TRACE;
	return 0;
}

int pinctrl_pm_select_default_state(struct device *dev)
{
	TRACE;
	return -1;
}

int pinctrl_pm_select_sleep_state(struct device *dev)
{
	TRACE;
	return -1;
}

struct resource *platform_get_resource(struct platform_device * d, unsigned r1, unsigned r2)
{
	TRACE;
	return nullptr;
}

void pm_runtime_enable(struct device *dev)
{
	TRACE;
}

void pm_runtime_get_noresume(struct device *dev)
{
	TRACE;
}

int  pm_runtime_set_active(struct device *dev)
{
	TRACE;
	return 0;
}

void pm_runtime_set_autosuspend_delay(struct device *dev, int delay)
{
	TRACE;
}

void pm_runtime_use_autosuspend(struct device *dev)
{
	TRACE;
}

struct ptp_clock *ptp_clock_register(struct ptp_clock_info *info, struct device *parent)
{
	TRACE;
	return (ptp_clock*)0xdeadbeef;
}

int regulator_enable(struct regulator * d)
{
	TRACE;
	return 0;
}

int class_register(struct class_ *cls)
{
	TRACE;
	return 0;
}

int try_module_get(struct module *mod)
{
	TRACE;
	return -1;
}

struct device *get_device(struct device *dev)
{
	TRACE;
	return dev;
}

int device_bind_driver(struct device *dev)
{
	TRACE;
	return 0;
}

void netif_tx_start_all_queues(struct net_device *dev)
{
	TRACE;
}

void netif_tx_lock_bh(struct net_device *dev)
{
	TRACE;
}

int  device_set_wakeup_enable(struct device *dev, bool enable)
{
	TRACE;
	return 0;
}

void trace_consume_skb(struct sk_buff * sb)
{
	TRACE;
}

void trace_kfree_skb(struct sk_buff * sb, void * p)
{
	TRACE;
}

void netif_tx_unlock_bh(struct net_device *dev)
{
	TRACE;
}

void netif_wake_queue(struct net_device * d)
{
	TRACE;
}

bool netdev_uses_dsa(struct net_device *dev)
{
	TRACE;
	return false;
}

void dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
                                size_t size, enum dma_data_direction dir)
{
	TRACE;
}

void dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr, size_t size,
                             enum dma_data_direction dir)
{
	TRACE;
}

void of_node_put(struct device_node *node)
{
	TRACE;
}

const void *of_get_mac_address(struct device_node *np)
{
	TRACE;
	return nullptr;
}

void rtnl_lock(void)
{
	TRACE;
}

void rtnl_unlock(void)
{
	TRACE;
}

int request_module(const char *fmt, ...)
{
	TRACE;
	return 0;
}

int  bus_register(struct bus_type *bus)
{
	TRACE;
	return 0;
}

}
