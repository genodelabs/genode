/*
 * \brief  Emulate 'pci_dev' structure
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-04-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode base includes */
#include <base/object_pool.h>
#include <io_mem_session/client.h>
#include <irq_session/connection.h>
#include <ram_session/client.h>

/* Genode os includes */
#include <io_port_session/client.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <util/volatile_object.h>

/* Linux includes */
#include <extern_c_begin.h>
#include <lx_emul.h>
#include <extern_c_end.h>
#include <platform/lx_mem.h>

struct  bus_type pci_bus_type;

using namespace Genode;

class Io_port
{

	private:

		unsigned                                     _base = 0;
		unsigned                                     _size = 0;
		Io_port_session_capability                   _cap;
		Lazy_volatile_object<Io_port_session_client> _port;

		bool _valid(unsigned port) {
			return _cap.valid() && port >= _base && port < _base + _size; }

	public:

		~Io_port()
		{
			if (_cap.valid())
				_port.destruct();
		}

		void session(unsigned base, unsigned size, Io_port_session_capability cap)
		{
			_base = base;
			_size = size;
			_cap  = cap;
			_port.construct(_cap);
		}

		template<typename POD>
		bool out(unsigned port, POD val)
		{
			if (!_valid(port))
				return false;

			switch (sizeof(POD)) {
				case 1: _port->outb(port, val); break;
				case 2: _port->outw(port, val); break;
				case 4: _port->outl(port, val); break;
			}

			return true;
		}

		template<typename POD>
		bool in(unsigned port, POD *val)
		{
			if (!_valid(port))
				return false;;

			switch (sizeof(POD)) {
				case 1: *val = _port->inb(port); break;
				case 2: *val = _port->inw(port); break;
				case 4: *val = _port->inl(port); break;
			}

			return true;
		}
};


/**
 * Virtual IRQ number (monotonically increasing)
 */
static unsigned virq_num()
{
	static unsigned instance = 128;
	return ++instance;
}

/**
 * Scan PCI bus and probe for HCDs
 */
class Pci_driver : public Genode::List<Pci_driver>::Element
{
	private:

		pci_driver            *_drv;  /* Linux PCI driver */
		Platform::Device_capability _cap;  /* PCI cap */
		pci_device_id const   *_id;   /* matched id for this driver */
		Io_port                _port;

	public:

		pci_dev               *_dev;  /* Linux PCI device */

	private:

		/* offset used in PCI config space */
		enum Pci_config { IRQ = 0x3c, REV = 0x8, CMD = 0x4 };

		/**
		 * Fill Linux device informations
		 */
		void _setup_pci_device()
		{
			using namespace Platform;

			Device_client client(_cap);
			uint8_t bus, dev, func;
			client.bus_address(&bus, &dev, &func);

			_dev = new (Genode::env()->heap()) pci_dev;
			_dev->devfn        = ((uint16_t)bus << 8) | (0xff & PCI_DEVFN(dev, func));
			_dev->vendor       = client.vendor_id();
			_dev->device       = client.device_id();
			_dev->class_       = client.class_code();
			_dev->revision     = client.config_read(REV, Device::ACCESS_8BIT);
			_dev->dev.driver   = &_drv->driver;

			/* dummy dma mask used to mark device as DMA capable */
			static u64 dma_mask = ~(u64)0;
			_dev->dev.dma_mask = &dma_mask;
			_dev->dev.coherent_dma_mask = ~0;

			/*
			 * We initialize the IRQ line with a virtual number to ensure that
			 * each device gets a unique number and, hence, requests its
			 * associated IRQ capability at the platform driver. Consequently,
			 * we give a away the questionable option to implement IRQ sharing
			 * locally and leave it to the platform driver, which just uses
			 * signalling anyway.
			 */
			_dev->irq = virq_num();

			/* hide ourselfs in  bus structure */
			_dev->bus          = (struct pci_bus *)this;

			/* setup resources */
			bool io = false;
			for (unsigned i = 0; i < Device::NUM_RESOURCES; i++) {
				Device::Resource res = client.resource(i);
				_dev->resource[i].start = res.base();
				_dev->resource[i].end   = res.base() + res.size() - 1;

				unsigned flags = 0;
				if (res.type() == Device::Resource::IO)     flags |= IORESOURCE_IO;
				if (res.type() == Device::Resource::MEMORY) flags |= IORESOURCE_MEM;
				_dev->resource[i].flags = flags;

				/* request port I/O session */
				if (res.type() == Device::Resource::IO) {
					uint8_t virt_bar = client.phys_bar_to_virt(i);
					_port.session(res.base(), res.size(), client.io_port(virt_bar));
					io = true;
					lx_log(DEBUG_PCI, "I/O [%u-%u)",
					            res.base(), res.base() + res.size());
				}

				/* request I/O memory (write combined) */
				if (res.type() == Device::Resource::MEMORY)
					lx_log(DEBUG_PCI, "I/O memory [%x-%x)", res.base(),
					            res.base() + res.size());
			}

			/* enable bus master and io bits */
			uint16_t cmd = client.config_read(CMD, Device::ACCESS_16BIT);
			cmd |= io ? 0x1 : 0;

			/* enable bus master */
			cmd |= 0x4;
			client.config_write(CMD, cmd, Device::ACCESS_16BIT);

			_drivers().insert(this);
		}

		/**
		 * Probe device with driver
		 */
		bool _probe()
		{
			_setup_pci_device();

			if (!_drv->probe(_dev, _id))
				return true;

			PERR("Probe failed\n");
			return false;
		}

		template <typename T>
		Platform::Device::Access_size _access_size(T t)
		{
			switch (sizeof(T))
			{
				case 1:
					return Platform::Device::ACCESS_8BIT;
				case 2:
					return Platform::Device::ACCESS_16BIT;
				default:
					return Platform::Device::ACCESS_32BIT;
			}
		}

		static Genode::List<Pci_driver> & _drivers()
		{
			static Genode::List<Pci_driver> _list;
			return _list;
		}

	public:

		Pci_driver(pci_driver *drv, Platform::Device_capability cap,
		           pci_device_id const * id)
		: _drv(drv), _cap(cap), _id(id), _dev(0)
		{
			if (!_probe())
				throw -1;
		}

		~Pci_driver()
		{
			if (!_dev)
				return;

			_drivers().remove(this);

			destroy(Genode::env()->heap(), _dev);
		}

		/**
		 * Read/write data from/to config space
		 */
		template <typename T>
		void config_read(unsigned int devfn, T *val)
		{
			Platform::Device_client client(_cap);
			*val = client.config_read(devfn, _access_size(*val));
		}

		template <typename T>
		void config_write(unsigned int devfn, T val)
		{
			Platform::Device_client client(_cap);
			client.config_write(devfn, val, _access_size(val));
		}

		static Genode::Irq_session_capability irq_cap(unsigned irq)
		{
			for (Pci_driver *d = _drivers().first(); d; d = d->next()) {
				if (d->_dev && d->_dev->irq != irq)
					continue;

				Platform::Device_client client(d->_cap);
				return client.irq(0);
			}

			return Genode::Irq_session_capability();
		}

		static Genode::Io_mem_session_capability io_mem(resource_size_t phys) {

			for (Pci_driver *d = _drivers().first(); d; d = d->next()) {
				if (!d->_dev)
					continue;

				unsigned bar = 0;
				for (; bar < PCI_ROM_RESOURCE; bar++) {
					if ((pci_resource_flags(d->_dev, bar) & IORESOURCE_MEM) &&
					    (pci_resource_start(d->_dev, bar) == phys))
						break;
				}
				if (bar >= PCI_ROM_RESOURCE)
					continue;

				Platform::Device_client client(d->_cap);
				return client.io_mem(bar);
			}

			PERR("Device using i/o memory of address %zx is unknown", phys);
			return Genode::Io_mem_session_capability();
		}

		template <typename POD, bool READ = true> 
		static POD port_io(unsigned port, POD val = 0)
		{
			for (Pci_driver *d = _drivers().first(); d; d = d->next()) {
				if (!d->_dev)
					continue;

				if (READ && d->_port.in<POD>(port, &val))
					return val;
				else if (!READ && d->_port.out<POD>(port, val))
					return (POD)~0;
			}

			return (POD)~0;
		}
};

/********************************
 ** Backend memory definitions **
 ********************************/

struct Memory_object_base : Genode::Object_pool<Memory_object_base>::Entry
{
	Memory_object_base(Genode::Ram_dataspace_capability cap)
	: Genode::Object_pool<Memory_object_base>::Entry(cap) {}
	virtual ~Memory_object_base() {};

	virtual void free() = 0;

	Genode::Ram_dataspace_capability ram_cap()
	{
		using namespace Genode;
		return reinterpret_cap_cast<Ram_dataspace>(cap());
	}
};


struct Ram_object : Memory_object_base
{
	Ram_object(Genode::Ram_dataspace_capability cap)
	: Memory_object_base(cap) {}

	void free();
};


struct Dma_object : Memory_object_base
{
	Dma_object(Genode::Ram_dataspace_capability cap)
	: Memory_object_base(cap) {}

	void free();
};


/*********************
 ** Linux interface **
 *********************/

static Platform::Connection pci;
static Genode::Object_pool<Memory_object_base> memory_pool;

int pci_register_driver(struct pci_driver *drv)
{
	lx_log(DEBUG_PCI, "DRIVER name: %s", drv->name);
	drv->driver.name = drv->name;

	pci_device_id const  *id = drv->id_table;
	if (!id)
		return -ENODEV;

	using namespace Genode;

	bool found = false;

	while (id->class_ || id->class_mask || id->class_) {

		if (id->class_ == (unsigned)PCI_ANY_ID) {
			lx_log(DEBUG_PCI, "Skipping PCI_ANY_ID device class");
			id++;
			continue;
		}

		Genode::env()->parent()->upgrade(pci.cap(), "ram_quota=4096");

		Platform::Device_capability cap = pci.first_device(id->class_,
		                                                   id->class_mask);
		while (cap.valid()) {

			if (DEBUG_PCI) {
				uint8_t bus, dev, func;
				Platform::Device_client client(cap);
				client.bus_address(&bus, &dev, &func);
				lx_log(DEBUG_PCI, "bus: %x  dev: %x func: %x", bus, dev, func);
			}

			Pci_driver *pci_drv = 0;
			try {
				/* probe device */
				pci_drv = new (env()->heap()) Pci_driver(drv, cap, id);
				pci.on_destruction(Platform::Connection::KEEP_OPEN);
				found = true;

			} catch (Platform::Device::Quota_exceeded) {
				Genode::env()->parent()->upgrade(pci.cap(), "ram_quota=4096");
				continue;
			} catch (...) {
				destroy(env()->heap(), pci_drv);
				pci_drv = 0;
			}

			Platform::Device_capability free_up = cap;

			try {
				cap = pci.next_device(cap, id->class_, id->class_mask);
			} catch (Platform::Device::Quota_exceeded) {
				Genode::env()->parent()->upgrade(pci.cap(), "ram_quota=4096");
				cap = pci.next_device(cap, id->class_, id->class_mask);
			}

			if (!pci_drv)
				pci.release_device(free_up);
		}
		id++;
	}

	return found ? 0 : -ENODEV;
}


size_t pci_resource_start(struct pci_dev *dev, unsigned bar)
{
	if (bar >= DEVICE_COUNT_RESOURCE)
		return 0;

	return dev->resource[bar].start;
}


size_t pci_resource_len(struct pci_dev *dev, unsigned bar)
{
	size_t start = pci_resource_start(dev, bar);

	if (!start)
		return 0;

	return (dev->resource[bar].end - start) + 1;
}


unsigned int pci_resource_flags(struct pci_dev *dev, unsigned bar)
{
	if (bar >= DEVICE_COUNT_RESOURCE)
		return 0;

	return dev->resource[bar].flags;
}


int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int, int where, u8 *val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	drv->config_read(where, val);
	lx_log(DEBUG_PCI, "READ %p: where: %x val: %x", drv, where, *val);
	return 0;
}


int pci_bus_read_config_word(struct pci_bus *bus, unsigned int, int where, u16 *val)
{ 
	Pci_driver *drv = (Pci_driver *)bus;
	drv->config_read(where, val);
	lx_log(DEBUG_PCI, "READ %p: where: %x val: %x", drv, where, *val);
	return 0; 
}


int pci_bus_write_config_word(struct pci_bus *bus, unsigned int, int where, u16 val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	lx_log(DEBUG_PCI, "WRITE %p: where: %x val: %x", drv, where, val);
	drv->config_write(where, val);
	return 0;
}


int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int, int where, u8 val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	lx_log(DEBUG_PCI, "WRITE %p: where: %x val: %x", drv, where, val);
	drv->config_write(where, val);
	return 0;
}


const char *pci_name(const struct pci_dev *pdev)
{
	/* simply return driver name */
	return "dummy";
}


void Ram_object::free() { Genode::env()->ram_session()->free(ram_cap()); }


void Dma_object::free() { pci.free_dma_buffer(ram_cap()); }


Genode::Ram_dataspace_capability
Backend_memory::alloc(Genode::addr_t size, Genode::Cache_attribute cached)
{
	using namespace Genode;

	Memory_object_base *o;
	Genode::Ram_dataspace_capability cap;
	if (cached == CACHED) {
		cap = env()->ram_session()->alloc(size);
		o = new (env()->heap())	Ram_object(cap);
	} else {
		/* transfer quota to pci driver, otherwise it will give us a exception */
		char buf[32];
		Genode::snprintf(buf, sizeof(buf), "ram_quota=%ld", size);
		Genode::env()->parent()->upgrade(pci.cap(), buf);

		cap = pci.alloc_dma_buffer(size);
		o = new (env()->heap()) Dma_object(cap);
	}

	memory_pool.insert(o);
	return cap;
}


void Backend_memory::free(Genode::Ram_dataspace_capability cap)
{
	using namespace Genode;

	Memory_object_base *object;
	auto lambda = [&] (Memory_object_base *o) {
		object = o;

		if (object) {
			object->free();
			memory_pool.remove(object);
		}
	};
	memory_pool.apply(cap, lambda);
	destroy(env()->heap(), object);
}


/**********************
 ** asm-generic/io.h **
 **********************/

void outb(u8  value, u32 port) { Pci_driver::port_io<u8,  false>(port, value); }
void outw(u16 value, u32 port) { Pci_driver::port_io<u16, false>(port, value); }
void outl(u32 value, u32 port) { Pci_driver::port_io<u32, false>(port, value); }

u8  inb(u32 port) { return Pci_driver::port_io<u8>(port);  }
u16 inw(u32 port) { return Pci_driver::port_io<u16>(port); }
u32 inl(u32 port) { return Pci_driver::port_io<u32>(port); }


/*****************************************
 ** Platform specific irq cap discovery **
 *****************************************/

Genode::Irq_session_capability platform_irq_activate(int irq)
{
	return Pci_driver::irq_cap(irq);
}

/******************
 ** MMIO regions **
 ******************/

class Mem_range : public Genode::Io_mem_session_client
{
	private:

		Genode::Io_mem_dataspace_capability _ds;
		Genode::addr_t                      _vaddr;

	public:

		Mem_range(Genode::addr_t base,
		          Genode::Io_mem_session_capability io_cap)
		:
			Io_mem_session_client(io_cap), _ds(dataspace()), _vaddr(0UL)
		{
			_vaddr  = Genode::env()->rm_session()->attach(_ds);
			_vaddr |= base & 0xfffUL;
		}

		Genode::addr_t vaddr()     const { return _vaddr; }
};


void *ioremap(resource_size_t phys_addr, unsigned long size)
{
	Mem_range  * io_mem  = new (Genode::env()->heap()) Mem_range(phys_addr, Pci_driver::io_mem(phys_addr));
	if (io_mem->vaddr())
		return (void *)io_mem->vaddr();

	PERR("Failed to request I/O memory: [%zx,%lx)", phys_addr,
	     phys_addr + size);

	return nullptr;
}
