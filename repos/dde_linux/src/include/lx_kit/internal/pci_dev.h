/*
 * \brief  Emulate 'pci_dev' structure
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__INTERNAL__PCI_DEV_H_
#define _LX_KIT__INTERNAL__PCI_DEV_H_

/* Genode includes */
#include <base/object_pool.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <io_mem_session/connection.h>
#include <base/attached_dataspace.h>
#include <util/retry.h>

/* Linux emulation environment includes */
#include <lx_kit/internal/list.h>
#include <lx_kit/internal/io_port.h>

namespace Lx {

	class Pci_dev;

	/**
	 * Return singleton 'Platform::Connection'
	 *
	 * Implementation must be privided by the driver.
	 */
	Platform::Connection *pci();

	template <typename FUNC>
	static inline void for_each_pci_device(FUNC const &func);
}


class Lx::Pci_dev : public pci_dev, public Lx_kit::List<Pci_dev>::Element
{
	private:

		bool const verbose = true;

		Platform::Device_client _client;

		Io_port _io_port;
		Genode::Io_mem_session_capability _io_mem[DEVICE_COUNT_RESOURCE];

		/* offset used in PCI config space */
		enum Pci_config { IRQ = 0x3c, REV = 0x8, CMD = 0x4,
		                  STATUS = 0x4, CAP = 0x34 };
		enum Pci_cap    { CAP_LIST = 0x10, CAP_EXP = 0x10,
		                  CAP_EXP_FLAGS = 0x2, CAP_EXP_DEVCAP = 0x4 };

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

		static unsigned _virq_num()
		{
			static unsigned instance = 128;
			return ++instance;
		}

	public:

		/**
		 * Constructor
		 */
		Pci_dev(Platform::Device_capability cap)
		:
			_client(cap)
		{
			using namespace Platform;

			Genode::memset(static_cast<pci_dev *>(this), 0, sizeof(pci_dev));

			this->vendor   = _client.vendor_id();
			this->device   = _client.device_id();
			this->class_   = _client.class_code();
			this->revision = _client.config_read(REV, Device::ACCESS_8BIT);

			/* dummy dma mask used to mark device as DMA capable */
			this->dev._dma_mask_buf = ~(u64)0;
			this->dev.dma_mask = &this->dev._dma_mask_buf;
			this->dev.coherent_dma_mask = ~0;

			enum { USB_SUB_CLASS = 0xc0300 };

			/*
			 * For USB we generate virtual IRQ numbers so we can identify the device
			 * later on
			 */
			if ((class_ & ~0xffU) == USB_SUB_CLASS)
				this->irq = _virq_num();
			else
				/* read interrupt line */
				this->irq = _client.config_read(IRQ, Device::ACCESS_8BIT);


			/* hide ourselfs in  bus structure */
			this->bus = (struct pci_bus *)this;

			/* setup resources */
			bool io = false;
			bool mem = false;
			for (int i = 0; i < Device::NUM_RESOURCES; i++) {
				Device::Resource res = _client.resource(i);
				if (res.type() == Device::Resource::INVALID)
					continue;

				this->resource[i].start = res.base();
				this->resource[i].end   = res.base() + res.size() - 1;

				unsigned flags = 0;
				if (res.type() == Device::Resource::IO)     flags |= IORESOURCE_IO;
				if (res.type() == Device::Resource::MEMORY) flags |= IORESOURCE_MEM;
				this->resource[i].flags = flags;

				/* request port I/O session */
				if (res.type() == Device::Resource::IO) {
					uint8_t const virt_bar = _client.phys_bar_to_virt(i);
					_io_port.session(res.base(), res.size(), _client.io_port(virt_bar));
					io = true;
				}
				if (res.type() == Device::Resource::MEMORY)
					mem = true;
			}

			/* enable bus master, memory and io bits */
			uint16_t cmd = _client.config_read(CMD, Device::ACCESS_16BIT);
			cmd |= io ? 0x1 : 0;
			cmd |= mem ? 0x2 : 0;

			/* enable bus master */
			cmd |= 0x4;
			config_write(CMD, cmd);

			/* get pci express capability */
			this->pcie_cap = 0;
			uint16_t status = _client.config_read(STATUS, Device::ACCESS_32BIT) >> 16;
			if (status & CAP_LIST) {
				uint8_t offset = _client.config_read(CAP, Device::ACCESS_8BIT);
				while (offset != 0x00) {
					uint8_t value = _client.config_read(offset, Device::ACCESS_8BIT);

					if (value == CAP_EXP)
						this->pcie_cap = offset;

					offset = _client.config_read(offset + 1, Device::ACCESS_8BIT);
				}
			}

			if (this->pcie_cap) {
				uint16_t reg_val = _client.config_read(this->pcie_cap, Device::ACCESS_16BIT);
				this->pcie_flags_reg = reg_val;
			}
		}

		/**
		 * Read/write data from/to config space
		 */
		template <typename T>
		void config_read(unsigned int devfn, T *val)
		{
			*val = _client.config_read(devfn, _access_size(*val));
		}

		template <typename T>
		void config_write(unsigned int devfn, T val)
		{
			pci()->with_upgrade([&] () {
				_client.config_write(devfn, val, _access_size(val)); });
		}

		Platform::Device &client() { return _client; }

		Io_port &io_port() { return _io_port; }

		Genode::Io_mem_session_capability io_mem(unsigned bar,
		                                         Genode::Cache_attribute cache_attribute)
		{
			if (bar >= DEVICE_COUNT_RESOURCE)
				return Genode::Io_mem_session_capability();

			if (!_io_mem[bar].valid())
				_io_mem[bar] = _client.io_mem(_client.phys_bar_to_virt(bar),
				                              cache_attribute);

			return _io_mem[bar];
		}
};


/**
 * Call functor for each available physical PCI device
 *
 * The functor is called with the device capability as argument. If
 * returns true if we can stop iterating. In this case, the device
 * is expected to be acquired by the driver. All other devices are
 * released at the platform driver.
 */
template <typename FUNC>
void Lx::for_each_pci_device(FUNC const &func)
{
	/*
	 * Obtain first device, the operation may exceed the session quota.
	 * So we use the 'retry' mechanism.
	 */
	Platform::Device_capability cap =
		Lx::pci()->with_upgrade([&] () {
			return Lx::pci()->first_device(); });

	/*
	 * Iterate over the devices of the platform session.
	 */
	while (cap.valid()) {

		/*
		 * Call functor, stop iterating depending on its return value.
		 */
		if (func(cap))
			break;

		/*
		 * Release current device and try next one. Upgrade session
		 * quota on demand.
		 */
		Platform::Device_capability next_cap =
			Lx::pci()->with_upgrade([&] () {
				return pci()->next_device(cap); });

		Lx::pci()->release_device(cap);
		cap = next_cap;
	}
}

#endif /* _LX_KIT__INTERNAL__PCI_DEV_H_ */
