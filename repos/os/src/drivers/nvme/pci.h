/*
 * \brief  NVMe PCIe backend
 * \author Josef Soentgen
 * \date   2018-03-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NVME_PCI_H_
#define _NVME_PCI_H_

/* Genode includes */
#include <irq_session/connection.h>
#include <platform_device/client.h>
#include <platform_session/connection.h>


namespace Nvme {

	using namespace Genode;

	struct Pci;
}


struct Nvme::Pci : Platform::Connection,
                   Util::Dma_allocator
{
	struct Missing_controller : Genode::Exception { };

	enum {
		CLASS_MASS_STORAGE = 0x010000u,
		CLASS_MASK         = 0xffff00u,
		SUBCLASS_NVME      = 0x000800u,
		NVME_DEVICE        = CLASS_MASS_STORAGE | SUBCLASS_NVME,
		NVME_PCI           = 0x02,
		NVME_BASE_ID       = 0,
	};

	enum Pci_config { IRQ = 0x3c, CMD = 0x4, CMD_IO = 0x1,
	                  CMD_MEMORY = 0x2, CMD_MASTER = 0x4 };

	Platform::Device::Resource                     _res        { };
	Platform::Device_capability                    _device_cap { };
	Genode::Constructible<Platform::Device_client> _device     { };

	Genode::Constructible<Genode::Irq_session_client> _irq { };

	/**
	 * Constructor
	 */
	Pci(Genode::Env &env) : Platform::Connection(env)
	{
		upgrade_ram(2*4096u);
		upgrade_caps(8);

		_device_cap = with_upgrade([&] () {
			return next_device(_device_cap,
			                   NVME_DEVICE, CLASS_MASK);
		});

		if (!_device_cap.valid()) { throw Missing_controller(); }

		_device.construct(_device_cap);

		_res = _device->resource(NVME_BASE_ID);

		uint16_t cmd  = _device->config_read(Pci_config::CMD, Platform::Device::ACCESS_16BIT);
		         cmd |= 0x2; /* respond to memory space accesses */
		         cmd |= 0x4; /* enable bus master */

		_device->config_write(Pci_config::CMD, cmd, Platform::Device::ACCESS_16BIT);

		_irq.construct(_device->irq(0));

		Genode::log("NVMe PCIe controller found (",
		            Genode::Hex(_device->vendor_id()), ":",
		            Genode::Hex(_device->device_id()), ")");
	}

	/**
	 * Return base address of controller MMIO region
	 */
	addr_t base() const { return _res.base(); }

	/**
	 * Return size of controller MMIO region
	 */
	size_t size() const { return _res.size(); }

	/**
	 * Set interrupt signal handler
	 *
	 * \parm sigh  signal capability
	 */
	void sigh_irq(Genode::Signal_context_capability sigh)
	{
		_irq->sigh(sigh);
		_irq->ack_irq();
	}

	/**
	 * Acknowledge interrupt
	 */
	void ack_irq() { _irq->ack_irq(); }

	/*****************************
	 ** Dma_allocator interface **
	 *****************************/

	/**
	 * Allocator DMA buffer
	 *
	 * \param size  size of the buffer
	 *
	 * \return Ram_dataspace_capability
	 */
	Genode::Ram_dataspace_capability alloc(size_t size) override
	{
		size_t donate = size;
		return retry<Out_of_ram>(
			[&] () {
				return retry<Out_of_caps>(
					[&] () { return Pci::Connection::alloc_dma_buffer(size); },
					[&] () { upgrade_caps(2); });
			},
			[&] () {
				upgrade_ram(donate);
				donate = donate * 2 > size ? 4096 : donate * 2;
			});
	}

	/**
	 * Free DMA buffer
	 *
	 * \param cap  RAM dataspace capability
	 */
	void free(Genode::Ram_dataspace_capability cap) override
	{
		Pci::Connection::free_dma_buffer(cap);
	}
};

#endif /* _NVME_PCI_H_ */
