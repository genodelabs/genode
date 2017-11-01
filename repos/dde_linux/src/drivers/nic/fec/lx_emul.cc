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
#include <base/env.h>
#include <base/snprintf.h>
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

		unsigned long _addr { 0 };
		struct page  *_page { nullptr };

		static Genode::List<Addr_to_page_mapping> & _list()
		{
			static Genode::List<Addr_to_page_mapping> _l;
			return _l;
		}

	public:

		Addr_to_page_mapping(unsigned long addr, struct page *page)
		: _addr(addr), _page(page) { }

		static void insert(struct page * page)
		{
			Addr_to_page_mapping *m = (Addr_to_page_mapping*)
				Lx::Malloc::mem().alloc(sizeof (Addr_to_page_mapping));
			m->_addr = (unsigned long)page->addr;
			m->_page = page;
			_list().insert(m);
		}

		static struct page * remove(unsigned long addr)
		{
			for (Addr_to_page_mapping *m = _list().first(); m; m = m->next())
				if (m->_addr == addr) {
					struct page * ret = m->_page;
					_list().remove(m);
					Lx::Malloc::mem().free(m);
					return ret;
				}

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

static Session_component * session = nullptr;

void Session_component::_register_session_component(Session_component & s) {
	session = &s; }


#include <lx_emul/extern_c_begin.h>
#include <linux/phy.h>
#include <linux/timecounter.h>
#include <lx_emul/extern_c_end.h>

extern "C" {

void lx_backtrace()
{
	Genode::backtrace();
}


int platform_driver_register(struct platform_driver * drv)
{
	static platform_device pd_fec;
	static const char * name = "2188000.ethernet";
	pd_fec.name = name;
	return drv->probe(&pd_fec);
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

	static const struct ethtool_ops default_ethtool_ops { };
	if (!dev->ethtool_ops) dev->ethtool_ops = &default_ethtool_ops;

	dev->dev_addr = (unsigned char*) kzalloc(ETH_ALEN, GFP_KERNEL);

	return dev;
}


const struct of_device_id *of_match_device(const struct of_device_id *matches,
                                           const struct device *dev)
{
	for (; matches && matches->compatible[0]; matches++)
		if (Genode::strcmp(matches->compatible, "fsl,imx6q-fec") == 0)
			return matches;

	return NULL;
}

void * devm_ioremap_resource(struct device *dev, struct resource *res)
{
	static Genode::Attached_io_mem_dataspace io_ds(Lx_kit::env().env(),
	                                        0x2188000, 0x1000);

	return io_ds.local_addr<void>();
}


void platform_set_drvdata(struct platform_device *pdev, void *data)
{
	pdev->dev.driver_data = data;
}

int of_get_phy_mode(struct device_node *np)
{
	for (int i = 0; i < PHY_INTERFACE_MODE_MAX; i++)
		if (!Genode::strcmp("rgmii", phy_modes((phy_interface_t)i)))
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

	static unsigned waechter = 0;
	ASSERT(!waechter++);

	*dma_handle = (dma_addr_t) addr;
	return addr;
}


void *dmam_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp)
{
	dma_addr_t dma_addr;
	void *addr = Lx::Malloc::dma().alloc(size, 12, &dma_addr);

	*dma_handle = dma_addr;
	return addr;
}


dma_addr_t dma_map_single(struct device *dev, void *cpu_addr, size_t size,
                          enum dma_data_direction)
{
	dma_addr_t dma_addr = (dma_addr_t) Lx::Malloc::dma().phys_addr(cpu_addr);
	
	if (dma_addr == ~0UL)
		Genode::error(__func__, ": virtual address ", cpu_addr,
		              " not registered for DMA");

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
	if (session) session->link_state(true);
}


void netif_carrier_off(struct net_device *dev)
{
	dev->state |= 1UL << __LINK_STATE_NOCARRIER;
	if (session) session->link_state(false);
}


int netif_device_present(struct net_device * d)
{
	TRACE;
	return 1;
}



int platform_get_irq(struct platform_device * d, unsigned int i)
{
	if (i > 1) return -1;

	return 150 + i;
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
		{ "ahb", 132*1000*1000 },
		{ "enet_clk_ref", 500*1000*1000 } };

	for (unsigned i = 0; i < (sizeof(clocks) / sizeof(struct clk)); i++)
		if (Genode::strcmp(clocks[i].name, id) == 0)
			return &clocks[i];

	return NULL;
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


static struct net_device * my_net_device = nullptr;


int register_netdev(struct net_device * d)
{
	my_net_device = d;

	d->state |= (1 << __LINK_STATE_START) | (1UL << __LINK_STATE_NOCARRIER);

	int err = d->netdev_ops->ndo_open(d);
	if (err) {
		Genode::error("ndo_open() failed: ", err);
		return err;
	}

	return 0;
}


struct net_device * fec_get_my_registered_net_device()
{
	return my_net_device;
}


void *kmem_cache_alloc_node(struct kmem_cache *cache, gfp_t, int)
{
	return (void*)cache->alloc();
}



struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page = (struct page *)kzalloc(sizeof(struct page), 0);

	size_t size = PAGE_SIZE << order;

	page->addr = Lx::Malloc::dma().alloc(size, 12);

	if (!page->addr) {
		Genode::error("alloc_pages: ", size, " failed");
		kfree(page);
		return 0;
	}

	Addr_to_page_mapping::insert(page);

	atomic_set(&page->_count, 1);

	return page;
}


void get_page(struct page *page)
{
	atomic_inc(&page->_count);
}


void *__alloc_page_frag(struct page_frag_cache *nc, unsigned int fragsz, gfp_t gfp_mask)
{
	struct page *page = alloc_pages(GFP_LX_DMA, fragsz / PAGE_SIZE);
	if (!page) return nullptr;
	return page->addr;
}


void __free_page_frag(void *addr)
{
	struct page *page = Addr_to_page_mapping::remove((unsigned long)addr);

	if (!atomic_dec_and_test(&page->_count))
		Genode::error("page reference count != 0");

	Lx::Malloc::dma().free(page->addr);
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


static void _completion_timeout(unsigned long t)
{
	Lx::Task *task = (Lx::Task *)t;
	task->unblock();
}


long __wait_completion(struct completion *work, unsigned long timeout)
{
	timer_list t;
	Lx::timer_update_jiffies();
	unsigned long j = timeout ? jiffies + timeout : 0;

	if (timeout) {
		setup_timer(&t, _completion_timeout, (unsigned long)Lx::scheduler().current());
		mod_timer(&t, timeout);
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

	return j ? j - jiffies : 1;
}


int request_module(const char *format, ...)
{
	TRACE;
	return 0;
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


int  bus_register(struct bus_type *bus)
{
	TRACE;
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
	if (session) session->unblock_rx_task(n);
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
	if (session) session->receive(skb);

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
	return NULL;
}

struct device_node *of_get_child_by_name( const struct device_node *node, const char *name)
{
	TRACE;
	return NULL;
}

struct netdev_queue *netdev_get_tx_queue(const struct net_device *dev, unsigned int index)
{
	TRACE;
	return NULL;
}

const void *of_get_property(const struct device_node *node, const char *name, int *lenp)
{
	TRACE;
	return NULL;
}

struct device_node *of_parse_phandle(const struct device_node *np, const char *phandle_name, int index)
{
	TRACE;
	return NULL;
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
	return NULL;
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
	return NULL;
}

int regulator_enable(struct regulator * d)
{
	TRACE;
	return 0;
}

int of_driver_match_device(struct device *dev, const struct device_driver *drv)
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
	return NULL;
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

}
