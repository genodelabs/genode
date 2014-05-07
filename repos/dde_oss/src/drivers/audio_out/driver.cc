/*
 * \brief  Driver probing and registration
 * \author Sebastian Sumpf
 * \date   2012-11-20
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <pci_session/connection.h>
#include <pci_device/client.h>

extern "C" {
#include <oss_config.h>
#include <dde_kit/pci.h>
}

#include <quirks.h>


/**
 * Driver management class
 */
class Driver
{
	private:

		enum {
			MAX_DRIVER = 10,
			PCI_CLASS_MASK  = 0xff0000,
			PCI_CLASS_MULTIMEDIA = 0x40000, /* class multimedia */
		};

		oss_driver *_drivers[MAX_DRIVER + 1]; /* registered drivers */

		Driver()
		{
			Genode::memset(_drivers, 0, sizeof(oss_driver *) * (MAX_DRIVER + 1));
			dde_kit_pci_init(PCI_CLASS_MULTIMEDIA, PCI_CLASS_MASK);
		}

		/**
		 * Match vendor product IDs
		 */
		bool _match(oss_driver *driver, Pci::Device_capability &cap)
		{
			Pci::Device_client client(cap);

			unsigned short vendor_id = client.vendor_id();
			unsigned short device_id = client.device_id();
			device_id_t *id_table = driver->id_table;

			for (int i = 0; id_table[i].vendor; i++) {
				if (id_table[i].vendor == vendor_id &&
				    id_table[i].product == device_id) {
					PINF("Found card: vendor 0x%x: product: 0x%x driver: %s",
					     vendor_id, device_id, driver->name);
					return true;
				}
			}

			return false;
		}

		/**
		 * Probe device with registered drivers
		 */
		oss_driver *_probe_driver(Pci::Device_capability &cap)
		{
			for (int i = 0; _drivers[i]; i++)
				if (_match(_drivers[i], cap))
					return _drivers[i];

			return  0;
		}

		/**
		 * Scan pci bus for sound devices
		 */
		Pci::Device_capability _scan_pci(Pci::Device_capability const &prev,
		                                 Pci::Connection &pci)
		{
			/*
			 * Just test for multimedia class, since some devices (e.g., Intel
			 * hda) do set the subclass to something different then audio (0x1).
			 */

			Pci::Device_capability cap;
			cap = pci.next_device(prev, PCI_CLASS_MULTIMEDIA, PCI_CLASS_MASK);
			if (prev.valid())
				pci.release_device(prev);
			return cap;
		}

	public:

		static Driver *d()
		{
			static Driver _d;
			return &_d;
		}

		/**
		 * Register drivers
		 */
		void add(oss_driver *driver)
		{
			for (int i = 0; i < MAX_DRIVER; i++)
				if (!_drivers[i]) {
					_drivers[i] = driver;
					return;
				}

			PWRN("Driver limit of %d reached", MAX_DRIVER);
		}

		/**
		 * Probe PCI devices with registered drivers
		 */
		void probe()
		{
			using namespace Genode;
			Pci::Connection pci;
			Pci::Device_capability cap;
			oss_driver *driver = 0;

			while ((cap = _scan_pci(cap, pci)).valid())
			{
				Pci::Device_client client(cap);

				if (!(driver = _probe_driver(cap)))
					continue;

				uint8_t bus, dev, func;
				client.bus_address(&bus, &dev, &func);

				/* setup oss device */
				oss_device_t *ossdev = new (env()->heap()) oss_device_t;
				Genode::memset(ossdev, 0, sizeof(oss_device_t));

				ossdev->bus = bus;
				ossdev->dev = dev;
				ossdev->fun = func;

				/* set I/O resources */
				for (int i = 0; i < Pci::Device::NUM_RESOURCES; i++) {
					Pci::Device::Resource res = client.resource(i);
					ossdev->res[i].base = res.base();
					ossdev->res[i].size = res.size();
					ossdev->res[i].io = res.type() == Pci::Device::Resource::IO ? 1 : 0;
				}

				ossdev->drv = driver;

				/* set quirks */
				setup_quirks(driver);
				driver->attach(ossdev);
			}
		}
};


/*****************
 ** C interface **
 *****************/

extern "C" void register_driver(struct oss_driver *driver)
{
	dde_kit_log(VERBOSE_OSS, "Register driver: %s", driver->name);
	Driver::d()->add(driver);
}


extern "C" void probe_drivers(void)
{
	Driver::d()->probe();
}

