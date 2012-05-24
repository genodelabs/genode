/*
 * \brief  Emulate 'pci_dev' structure
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-04-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode inludes */
#include <pci_session/connection.h>
#include <pci_device/client.h>

/* Linux includes */
#include <lx_emul.h>


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
		enum Pci_config { IRQ = 0x3c, REV = 0x8, CMD = 0x4 };

		/**
		 * Match class code of device with driver id
		 */
		bool _match(pci_device_id const *id)
		{
			Pci::Device_client client(_cap);
			if (!((id->device_class ^ client.class_code()) & id->class_mask)) {
				_id = id;
				return true;
			}

			return false;
		}

		/**
		 * Match supported device ID of driver to this device
		 */
		bool _match()
		{
			pci_device_id const  *id = _drv->id_table;

			if (!id)
				return false;

			while (id->vendor || id->subvendor || id->class_mask) {
				if (_match(id)) {
					dde_kit_log(DEBUG_PCI, "Device matched %p", this);
					return true;
				}
				id++;
			}

			return false;
		}

		/**
		 * Fill Linux device informations
		 */
		void _setup_pci_device()
		{
			using namespace Pci;

			_dev = new (Genode::env()->heap()) pci_dev;
			Device_client client(_cap);

			_dev->vendor       = client.vendor_id();
			_dev->device       = client.device_id();
			_dev->device_class = client.class_code();
			_dev->revision     = client.config_read(REV, Device::ACCESS_8BIT);
			_dev->dev.driver   = &_drv->driver;

			/* read interrupt line */
			_dev->irq          = client.config_read(IRQ, Device::ACCESS_8BIT);

			/* hide ourselfs in  bus structure */
			_dev->bus          = (struct pci_bus *)this;

			/* setup resources */
			bool io = false;
			for (int i = 0; i < Device::NUM_RESOURCES; i++) {
				Device::Resource res = client.resource(i);
				_dev->resource[i].start = res.base();
				_dev->resource[i].end   = res.base() + res.size() - 1;
				_dev->resource[i].flags = res.type() == Device::Resource::IO
				                        ? IORESOURCE_IO : 0;

				/* request port I/O session */
				if (res.type() == Device::Resource::IO) {
					if (dde_kit_request_io(res.base(), res.size()))
						PERR("Failed to request I/O: [%u,%u)",
						     res.base(), res.base() + res.size());
					io = true;
					dde_kit_log(DEBUG_PCI, "I/O [%u-%u)",
					            res.base(), res.base() + res.size());
				}

				/* request I/O memory (write combined) */
				if (res.type() == Device::Resource::MEMORY)
					dde_kit_log(DEBUG_PCI, "I/O memory [%x-%x)", res.base(),
					            res.base() + res.size());
			}

			/* enable bus master and io bits */
			uint16_t cmd = client.config_read(CMD, Device::ACCESS_16BIT);
			cmd |= io ? 0x1 : 0;

			/* enable bus master */
			cmd |= 0x4;
			client.config_write(CMD, cmd, Device::ACCESS_16BIT);
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

		Pci_driver(pci_driver *drv, Pci::Device_capability cap)
		: _drv(drv), _cap(cap), _id(0), _dev(0)
		{
			if (!_match())
				throw -1;

			if (!_probe())
				throw -2;
		}

		~Pci_driver()
		{
			if (!_dev)
				return;

			for (int i = 0; i < Pci::Device::NUM_RESOURCES; i++) {
				resource *r = &_dev->resource[i];

				if (r->flags & IORESOURCE_IO)
					dde_kit_release_io(r->start, (r->end - r->start) + 1);
			}

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


/*********************
 ** Linux interface **
 *********************/

int pci_register_driver(struct pci_driver *drv)
{
	dde_kit_log(DEBUG_PCI, "DRIVER name: %s", drv->name);
	drv->driver.name = drv->name;

	using namespace Genode;
	Pci::Connection pci;

	Pci::Device_capability cap = pci.first_device();
	Pci::Device_capability old;
	while (cap.valid()) {

		uint8_t bus, dev, func;
		Pci::Device_client client(cap);
		client.bus_address(&bus, &dev, &func);
		dde_kit_log(DEBUG_PCI, "bus: %x  dev: %x func: %x", bus, dev, func);

		Pci_driver *pci_drv= 0;
		try {
			pci_drv = new (env()->heap()) Pci_driver(drv, cap);
			pci.on_destruction(Pci::Connection::KEEP_OPEN);
			return 0;
		} catch (...) {
			destroy(env()->heap(), pci_drv);
			pci_drv = 0;
		}

		old = cap;
		cap = pci.next_device(cap);
		pci.release_device(old);
	}

	return -ENODEV;
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


int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn, int, u8 *val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	drv->config_read(devfn, val);
	dde_kit_log(DEBUG_PCI, "READ %p: %x", drv, *val);
	return 0;
}


int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn, int, u16 *val)
{ 
	Pci_driver *drv = (Pci_driver *)bus;
	drv->config_read(devfn, val);
	dde_kit_log(DEBUG_PCI, "READ %p: %x", drv, *val);
	return 0; 
}


int pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn, int, u16 val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	dde_kit_log(DEBUG_PCI, "WRITE %p: %x", drv, val);
	drv->config_write(devfn, val);
	return 0;
}


int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn, int, u8 val)
{
	Pci_driver *drv = (Pci_driver *)bus;
	dde_kit_log(DEBUG_PCI, "WRITE %p: %x", drv, val);
	drv->config_write(devfn, val);
	return 0;
}


const char *pci_name(const struct pci_dev *pdev)
{
	/* simply return driver name */
	return "dummy";
}

