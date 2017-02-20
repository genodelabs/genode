/*
 * \brief  Driver for PCI-bus platforms
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <irq_session/connection.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <util/reconstructible.h>

#include <ahci.h>


using namespace Genode;

struct X86_hba : Platform::Hba
{
	enum Pci_config {
		CLASS_MASS_STORAGE = 0x10000u,
		SUBCLASS_AHCI      = 0x600u,
		CLASS_MASK         = 0xffff00u,
		AHCI_DEVICE        = CLASS_MASS_STORAGE | SUBCLASS_AHCI,
		AHCI_BASE_ID       = 0x5,  /* resource id of ahci base addr <bar 5> */
		PCI_CMD            = 0x4,
	};

	Genode::Env &env;

	Platform::Connection                   pci { env };
	Platform::Device_capability            pci_device_cap;
	Constructible<Platform::Device_client> pci_device;
	Constructible<Irq_session_client>      irq;
	addr_t                                 res_base;
	size_t                                 res_size;

	X86_hba(Genode::Env &env) : env(env)
	{
		pci_device_cap = retry<Platform::Session::Out_of_metadata>(
			[&] () { return pci.next_device(pci_device_cap, AHCI_DEVICE,
				                            CLASS_MASK); },
			[&] () { pci.upgrade_ram(4096); });

		if (!pci_device_cap.valid()) {
			Genode::error("no AHCI controller found");
				throw -1;
		}

		/* construct pci client */
		pci_device.construct(pci_device_cap);
		Genode::log("AHCI found ("
		            "vendor: ", pci_device->vendor_id(), " "
		            "device: ", pci_device->device_id(), " "
		            "class: ", pci_device->class_code(), ")");

		/* read base address of controller */
		Platform::Device::Resource resource = pci_device->resource(AHCI_BASE_ID);
		res_base = resource.base();
		res_size = resource.size();

		/* enable bus master */
		uint16_t cmd = pci_device->config_read(PCI_CMD, Platform::Device::ACCESS_16BIT);
		cmd |= 0x4;
		_config_write(PCI_CMD, cmd, Platform::Device::ACCESS_16BIT);

		irq.construct(pci_device->irq(0));
	}

	void disable_msi()
	{
		enum { PM_CAP_OFF = 0x34, MSI_CAP = 0x5, MSI_ENABLED = 0x1 };
		uint8_t cap = pci_device->config_read(PM_CAP_OFF, Platform::Device::ACCESS_8BIT);

		/* iterate through cap pointers */
		for (uint16_t val = 0; cap; cap = val >> 8) {
			val = pci_device->config_read(cap, Platform::Device::ACCESS_16BIT);

			if ((val & 0xff) != MSI_CAP)
				continue;

			uint16_t msi = pci_device->config_read(cap + 2, Platform::Device::ACCESS_16BIT);

			if (msi & MSI_ENABLED) {
				_config_write(cap + 2, msi ^ MSI_CAP,
				              Platform::Device::ACCESS_8BIT);
				Genode::log("disabled MSI ", msi);
			}
		}
	}

	void _config_write(uint8_t op, uint16_t cmd,
	                   Platform::Device::Access_size width)
	{
		Genode::size_t donate = 4096;
		Genode::retry<Platform::Device::Quota_exceeded>(
			[&] () { pci_device->config_write(op, cmd, width); },
			[&] () {
				pci.upgrade_ram(donate);
				donate *= 2;
			});
	}


	/*******************
	 ** Hba interface **
	 *******************/

	Genode::addr_t base() const override { return res_base; }
	Genode::size_t size() const override { return res_size; }

	void sigh_irq(Signal_context_capability sigh) override
	{
		irq->sigh(sigh);
		ack_irq();
	}

	void ack_irq() override { irq->ack_irq(); }

	Ram_dataspace_capability
	alloc_dma_buffer(Genode::size_t size) override
	{
		size_t donate = size;

		return retry<Platform::Session::Out_of_metadata>(
			[&] () { return pci.alloc_dma_buffer(size); },
			[&] () {
				pci.upgrade_ram(donate);
				donate = donate * 2 > size ? 4096 : donate * 2;
			});
	}

	void free_dma_buffer(Genode::Ram_dataspace_capability ds)
	{
		pci.free_dma_buffer(ds);
	}
};


Platform::Hba &Platform::init(Genode::Env &env, Mmio::Delayer &)
{
	static X86_hba h(env);
	return h;
}
