/**
 * \brief  Driver for PCI-bus platforms
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <irq_session/connection.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <util/volatile_object.h>

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

	Platform::Connection                          pci;
	Platform::Device_capability                   pci_device_cap;
	Lazy_volatile_object<Platform::Device_client> pci_device;
	Lazy_volatile_object<Irq_session_client>      irq;
	addr_t                                        res_base;
	size_t                                        res_size;

	X86_hba()
	{
		for (unsigned i = 0; i < 2; i++)
			try {
				if (!(pci_device_cap =
				      pci.next_device(pci_device_cap, AHCI_DEVICE, CLASS_MASK)).valid()) {
					PERR("No AHCI controller found");
					throw -1;
				}
				break;
			} catch (Platform::Device::Quota_exceeded) {
				Genode::env()->parent()->upgrade(pci.cap(), "ram_quota=4096");
			}

		/* construct pci client */
		pci_device.construct(pci_device_cap);
		PINF("AHCI found (vendor: %04x device: %04x class:"
		     " %08x)\n", pci_device->vendor_id(),
		     pci_device->device_id(), pci_device->class_code());

		/* read base address of controller */
		Platform::Device::Resource resource = pci_device->resource(AHCI_BASE_ID);
		res_base = resource.base();
		res_size = resource.size();

		if (verbose)
			PDBG("base: %lx size: %zx", res_base, res_size);

		/* enable bus master */
		uint16_t cmd = pci_device->config_read(PCI_CMD, Platform::Device::ACCESS_16BIT);
		cmd |= 0x4;
		pci_device->config_write(PCI_CMD, cmd, Platform::Device::ACCESS_16BIT);

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
				pci_device->config_write(cap + 2, msi ^ MSI_CAP, Platform::Device::ACCESS_8BIT);
				PINF("Disabled MSIs %x", msi);
			}
		}
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
		/* transfer quota to pci driver, otherwise we get a exception */
		char quota[32];
		snprintf(quota, sizeof(quota), "ram_quota=%zd", size);
		env()->parent()->upgrade(pci.cap(), quota);

		return pci.alloc_dma_buffer(size);
	}

	void free_dma_buffer(Genode::Ram_dataspace_capability ds)
	{
		pci.free_dma_buffer(ds);
	}
};


Platform::Hba &Platform::init(Mmio::Delayer &)
{
	static X86_hba h;
	return h;
}
