/*
 * \brief  Emulate 'pci_dev' structure
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \author Josef Soentgen
 * \date   2012-04-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode inludes */
#include <ram_session/client.h>
#include <base/object_pool.h>
#include <pci_session/connection.h>
#include <pci_device/client.h>
#include <io_mem_session/connection.h>

/* local includes */
#include <lx.h>

#include <extern_c_begin.h>
# include <lx_emul.h>
#include <extern_c_end.h>


static bool const verbose = false;
#define PDBGV(...) do { if (verbose) PDBG(__VA_ARGS__); } while (0)


struct  bus_type pci_bus_type;


/**
 * Scan PCI bus and probe for HCDs
 */
class Pci_driver
{
	private:

		pci_driver            *_drv;  /* Linux PCI driver */
		Pci::Device_capability _cap;  /* PCI cap */
		pci_device_id const   *_id;   /* matched id for this driver */

	public:

		pci_dev               *_dev;  /* Linux PCI device */

	private:

		/* offset used in PCI config space */
		enum Pci_config { IRQ = 0x3c, REV = 0x8, CMD = 0x4,
		                  STATUS = 0x4, CAP = 0x34 };
		enum Pci_cap    { CAP_LIST = 0x10, CAP_EXP = 0x10,
		                  CAP_EXP_FLAGS = 0x2, CAP_EXP_DEVCAP = 0x4 };

		/**
		 * Fill Linux device informations
		 */
		bool _setup_pci_device()
		{
			using namespace Pci;

			Device_client client(_cap);
			if (client.device_id() != _id->device)
				return false;

			_dev = new (Genode::env()->heap()) pci_dev;

			_dev->vendor       = client.vendor_id();
			_dev->device       = client.device_id();
			_dev->class_       = client.class_code();
			_dev->revision     = client.config_read(REV, Device::ACCESS_8BIT);
			_dev->dev.driver   = &_drv->driver;

			/* dummy dma mask used to mark device as DMA capable */
			static u64 dma_mask = ~(u64)0;
			_dev->dev.dma_mask = &dma_mask;
			_dev->dev.coherent_dma_mask = ~0;

			/* read interrupt line */
			_dev->irq          = client.config_read(IRQ, Device::ACCESS_8BIT);

			/* hide ourselfs in  bus structure */
			_dev->bus          = (struct pci_bus *)this;

			/* setup resources */
			bool io = false;
			for (int i = 0; i < Device::NUM_RESOURCES; i++) {
				Device::Resource res = client.resource(i);
				if (res.type() == Device::Resource::INVALID)
					continue;

				_dev->resource[i].start = res.base();
				_dev->resource[i].end   = res.base() + res.size() - 1;
				_dev->resource[i].flags = res.type() == Device::Resource::IO
				                        ? IORESOURCE_IO : 0;

				PDBGV("base: %x size: %x type: %u",
				     res.base(), res.size(), res.type());

				/* request I/O memory (write combined) */
				if (res.type() == Device::Resource::MEMORY)
					PDBGV("I/O memory [%x-%x)", res.base(),
					     res.base() + res.size());
			}

			/* enable bus master and io bits */
			uint16_t cmd = client.config_read(CMD, Device::ACCESS_16BIT);
			cmd |= io ? 0x1 : 0;

			/* enable bus master */
			cmd |= 0x4;
			client.config_write(CMD, cmd, Device::ACCESS_16BIT);

			/* get pci express capability */
			_dev->pcie_cap = 0;
			uint16_t status = client.config_read(STATUS, Device::ACCESS_32BIT) >> 16;
			if (status & CAP_LIST) {
				uint8_t offset = client.config_read(CAP, Device::ACCESS_8BIT);
				while (offset != 0x00) {
					uint8_t value = client.config_read(offset, Device::ACCESS_8BIT);

					if (value == CAP_EXP)
						_dev->pcie_cap = offset;

					offset = client.config_read(offset + 1, Device::ACCESS_8BIT);
				}
			}

			if (_dev->pcie_cap) {
				uint16_t reg_val = client.config_read(_dev->pcie_cap, Device::ACCESS_16BIT);
				_dev->pcie_flags_reg = reg_val;
			}

			return true;
		}

		/**
		 * Probe device with driver
		 */
		bool _probe()
		{

			/* only probe if the device matches */
			if (!_setup_pci_device())
				return false;

			/* PDBG("probe: %p 0x%x", _dev, _id); */

			if (!_drv->probe(_dev, _id)) {
				return true;
			}

			return false;
		}

		template <typename T>
		Pci::Device::Access_size _access_size(T t)
		{
			switch (sizeof(T))
			{
				case 1:
					return Pci::Device::ACCESS_8BIT;
				case 2:
					return Pci::Device::ACCESS_16BIT;
				default:
					return Pci::Device::ACCESS_32BIT;
			}
		}

	public:

		Pci_driver(pci_driver *drv, Pci::Device_capability cap,
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

			destroy(Genode::env()->heap(), _dev);
		}

		/**
		 * Read/write data from/to config space
		 */
		template <typename T>
		void config_read(unsigned int devfn, T *val)
		{
			Pci::Device_client client(_cap);
			*val = client.config_read(devfn, _access_size(*val));
		}

		template <typename T>
		void config_write(unsigned int devfn, T val)
		{
			Pci::Device_client client(_cap);
			client.config_write(devfn, val, _access_size(val));
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

extern "C" { Pci::Device_capability pci_device_cap; }
static Pci::Connection *pci()
{
	static Pci::Connection _pci;
	return &_pci;
}
static Genode::Object_pool<Memory_object_base> memory_pool;


extern "C" int pci_register_driver(struct pci_driver *drv)
{
	drv->driver.name = drv->name;

	pci_device_id const  *id = drv->id_table;
	if (!id)
		return -ENODEV;

	using namespace Genode;

	enum {
		PCI_CLASS_MASK = 0xfff000,
		/**
		 * This is actually PCI_CLASS_NETWORK_OTHER and may only work
		 * for the iwlwifi driver.
		 */
		PCI_CLASS_WIFI = 0x028000,
	};

	unsigned found = 0;

	while (id->device) {
		if (id->class_ == (unsigned)PCI_ANY_ID) {
			id++;
			continue;
		}

		Pci::Device_capability cap = pci()->first_device(PCI_CLASS_WIFI,
		                                                 PCI_CLASS_MASK);

		while (cap.valid()) {
			Pci_driver *pci_drv = 0;
			try {
				pci_device_cap = cap;

				/* trigger that the device get be assigned to the wifi driver */
				pci()->config_extended(cap);

				/* probe device */
				pci_drv = new (env()->heap()) Pci_driver(drv, cap, id);
				pci()->on_destruction(Pci::Connection::KEEP_OPEN);
				found++;

			} catch (...) {
				destroy(env()->heap(), pci_drv);
				pci_drv = 0;
			}

			if (found)
				break;

			Pci::Device_capability free_up = cap;
			cap = pci()->next_device(cap, PCI_CLASS_WIFI, PCI_CLASS_MASK);
			if (!pci_drv)
				pci()->release_device(free_up);
		}
		id++;

		/* XXX */
		if (found)
			break;
	}

	return found ? 0 : -ENODEV;
}


extern "C" size_t pci_resource_start(struct pci_dev *dev, unsigned bar)
{
	if (bar >= DEVICE_COUNT_RESOURCE)
		return 0;

	return dev->resource[bar].start;
}


extern "C" size_t pci_resource_len(struct pci_dev *dev, unsigned bar)
{
	size_t start = pci_resource_start(dev, bar);

	if (!start)
		return 0;

	return (dev->resource[bar].end - start) + 1;
}


extern "C" void *pci_ioremap_bar(struct pci_dev *dev, int bar)
{
	using namespace Genode;

	size_t start = pci_resource_start(dev, bar);
	size_t size  = pci_resource_len(dev, bar);

	if (!start)
		return 0;

	Io_mem_connection *io_mem;
	try {
		io_mem = new (env()->heap()) Io_mem_connection(start, size, 0);
	} catch (...) {
		PERR("Failed to request I/O memory: [%zx,%lx)", start, start + size);
		return 0;
	}

	if (!io_mem->dataspace().valid()) {
		PERR("I/O memory not accessible");
		return 0;
	}

	addr_t map_addr = env()->rm_session()->attach(io_mem->dataspace());
	map_addr |= start & 0xfff;

	return (void*)map_addr;
}


extern "C" unsigned int pci_resource_flags(struct pci_dev *dev, unsigned bar)
{
	if (bar >= DEVICE_COUNT_RESOURCE)
		return 0;

	return dev->resource[bar].flags;
}


extern "C" int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int, int where, u8 *val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	drv->config_read(where, val);
	return 0;
}


extern "C" int pci_bus_read_config_word(struct pci_bus *bus, unsigned int, int where, u16 *val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	drv->config_read(where, val);
	return 0;
}


extern "C" int pci_bus_write_config_word(struct pci_bus *bus, unsigned int, int where, u16 val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	drv->config_write(where, val);
	return 0;
}


extern "C" int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int, int where, u8 val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	drv->config_write(where, val);
	return 0;
}


extern "C" const char *pci_name(const struct pci_dev *pdev)
{
	/* simply return driver name */
	return "dummy";
}


extern "C" int pcie_capability_read_word(struct pci_dev *pdev, int pos, u16 *val)
{
	Pci_driver *drv = (Pci_driver *)pdev->bus;
	switch (pos) {
	case PCI_EXP_LNKCTL:
		drv->config_read(pdev->pcie_cap + PCI_EXP_LNKCTL, val);
		return 0;
		break;
	default:
		break;
	}

	return 1;
}


void Ram_object::free() { Genode::env()->ram_session()->free(ram_cap()); }


void Dma_object::free() { pci()->free_dma_buffer(ram_cap()); }


Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached)
{
	using namespace Genode;

	Memory_object_base *o;
	Genode::Ram_dataspace_capability cap;
	if (cached == CACHED) {
		cap = env()->ram_session()->alloc(size);
		o = new (env()->heap())	Ram_object(cap);
	} else {
		cap = pci()->alloc_dma_buffer(size);
		o = new (env()->heap()) Dma_object(cap);
	}

	memory_pool.insert(o);
	return cap;
}


void Lx::backend_free(Genode::Ram_dataspace_capability cap)
{
	using namespace Genode;

	Memory_object_base *o = memory_pool.lookup_and_lock(cap);
	if (!o)
		return;

	o->free();
	memory_pool.remove_locked(o);
	destroy(env()->heap(), o);
}
