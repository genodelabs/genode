/*
 * \brief  PCI device configuration
 * \author Norman Feske
 * \date   2008-01-29
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _X86__PCI_DEVICE_CONFIG_H_
#define _X86__PCI_DEVICE_CONFIG_H_

#include <base/output.h>
#include <platform_device/platform_device.h>
#include <util/register.h>
#include "pci_config_access.h"


namespace Platform { namespace Pci { struct Resource; } }


class Platform::Pci::Resource
{
	public:

		struct Bar : Register<32>
		{
			struct Space : Bitfield<0,1> { enum { MEM = 0, PORT = 1 }; };

			struct Mem_type          : Bitfield<1,2>  { enum { MEM32 = 0, MEM64 = 2 }; };
			struct Mem_prefetch      : Bitfield<3,1>  { };
			struct Mem_address_mask  : Bitfield<4,28> { };
			struct Port_address_mask : Bitfield<2,14> { };

			static bool mem(access_t r)                           { return Space::get(r) == Space::MEM; }
			static bool mem64(access_t r)                         { return mem(r)
			                                                            && Mem_type::get(r) == Mem_type::MEM64; }
			static uint64_t mem_address(access_t r0, uint64_t r1) { return (r1 << 32) | Mem_address_mask::masked(r0); }
			static uint64_t mem_size(access_t r0, uint64_t r1)    { return ~mem_address(r0, r1) + 1; }

			static uint16_t port_address(access_t r) { return Port_address_mask::masked(r); }
			static uint16_t port_size(access_t r)    { return ~port_address(r) + 1; }
		};

	private:

		uint32_t _bar[2] { 0, 0 };  /* contains two consecutive BARs for MEM64 */
		uint64_t _size   { 0 };

	public:

	/* invalid resource */
	Resource() { }

	/* PORT or MEM32 resource */
	Resource(uint32_t bar, uint32_t size)
	:
		_bar{bar, 0}, _size(mem() ? Bar::mem_size(size, ~0) : Bar::port_size(size))
	{ }

	/* MEM64 resource */
	Resource(uint32_t bar0, uint32_t size0, uint32_t bar1, uint32_t size1)
	: _bar{bar0, bar1}, _size(Bar::mem_size(size0, size1))
	{ }

	bool valid()    const { return !!_bar[0]; } /* no base address -> invalid */
	bool mem()      const { return Bar::mem(_bar[0]); }
	uint64_t base() const { return mem() ? Bar::mem_address(_bar[0], _bar[1])
	                                     : Bar::port_address(_bar[0]); }
	uint64_t size() const { return _size; }

	Platform::Device::Resource api_resource()
	{
		/*
		 * The API type limits to 32-bit currently (defined in
		 * spec/x86/platform_device/platform_device.h)
		 */
		return Device::Resource((unsigned)_bar[0], (unsigned)_size);
	}

	void print(Output &out) const
	{
		Genode::print(out, Hex_range(base(), size()));
	}
};


namespace Platform {

	class Device_config
	{
		private:

			Pci::Bdf _bdf;

			/*
			 * Information provided by the PCI config space
			 */
			unsigned _vendor_id = 0, _device_id = 0;
			unsigned _class_code = 0;
			unsigned _header_type = 0;

			/*
			 * Header type definitions
			 */
			enum {
				HEADER_FUNCTION   = 0,
				HEADER_PCI_TO_PCI = 1,
				HEADER_CARD_BUS   = 2
			};

			Platform::Pci::Resource _resource[Device::NUM_RESOURCES];

			bool _resource_id_is_valid(int resource_id)
			{
				/*
				 * The maximum number of PCI resources depends on the
				 * header type of the device.
				 */
				int max_num = _header_type == HEADER_FUNCTION   ? Device::NUM_RESOURCES
				            : _header_type == HEADER_PCI_TO_PCI ? 2
				            : 0;

				return resource_id >= 0 && resource_id < max_num;
			}

			enum { INVALID_VENDOR = 0xffffU };

		public:

			enum { MAX_BUSES = 256, MAX_DEVICES = 32, MAX_FUNCTIONS = 8 };

			enum {
				PCI_CMD_REG    = 0x4,
				PCI_CMD_MASK   = 0x7, /* IOPORT (1), MEM(2), DMA(4) */
				PCI_CMD_DMA    = 0x4,
			};

			struct Pci_header : Pci::Config
			{
				Pci_header(Config_access &access, Pci::Bdf const bdf)
				: Pci::Config(access, bdf, 0 /* from start */) { }

				struct Command : Register<0x04, 16>
				{
					struct Ioport : Bitfield< 0, 1> { };
					struct Memory : Bitfield< 1, 1> { };
					struct Dma    : Bitfield< 2, 1> { };
				};
			};

			struct Device_bars {
				Pci::Bdf bdf;
				uint32_t bar_addr[Device::NUM_RESOURCES] { };

				bool all_invalid() const {
					for (unsigned i = 0; i < Device::NUM_RESOURCES; i++) {
						if (bar_addr[i] != 0 && bar_addr[i] != ~0U)
							return false;
					}
					return true;
				}

				Device_bars(Pci::Bdf bdf) : bdf(bdf) { }
				virtual ~Device_bars() { };
			};

			/**
			 * Constructor
			 */
			Device_config() : _bdf({ .bus = 0, .device = 0, .function = 0 }) {
				_vendor_id = INVALID_VENDOR; }

			Device_config(Pci::Bdf bdf) : _bdf(bdf) { }

			Device_config(Pci::Bdf bdf, Config_access *pci_config) : _bdf(bdf)
			{
				_vendor_id = pci_config->read(bdf, 0, Device::ACCESS_16BIT);

				/* break here if device is invalid */
				if (_vendor_id == INVALID_VENDOR)
					return;

				_device_id    = pci_config->read(bdf, 2, Device::ACCESS_16BIT);
				_class_code   = pci_config->read(bdf, 8, Device::ACCESS_32BIT) >> 8;
				_class_code  &= 0xffffff;
				_header_type  = pci_config->read(bdf, 0xe, Device::ACCESS_8BIT);
				_header_type &= 0x7f;

				/*
				 * We prevent scanning function 1-7 of non-multi-function
				 * devices by checking bit 7 (mf bit) of function 0 of the
				 * device. Note, the mf bit of function 1-7 is not significant
				 * and may be set or unset.
				 */
				if (bdf.function != 0) {
					Pci::Bdf const dev { .bus = bdf.bus, .device = bdf.device, .function = 0 };
					if (!(pci_config->read(dev, 0xe, Device::ACCESS_8BIT) & 0x80)) {
						_vendor_id = INVALID_VENDOR;
						return;
					}
				}

				/*
				 * We iterate over all BARs but check for 64-bit memory
				 * resources, which are stored in two consecutive BARs. The
				 * MEM64 information is stored in the first resource entry and
				 * the second resource is marked invalid.
				 */
				int i = 0;
				while (_resource_id_is_valid(i)) {

					using Pci::Resource;

					/* index of base-address register in configuration space */
					unsigned const bar_idx = 0x10 + 4 * i;

					/* read base-address register value */
					unsigned const bar_value =
						pci_config->read(bdf, bar_idx, Device::ACCESS_32BIT);

					/* skip invalid resource BARs */
					if (bar_value == ~0U || bar_value == 0U) {
						_resource[i] = Resource();
						++i;
						continue;
					}

					/*
					 * Determine resource size by writing a magic value (all
					 * bits set) to the base-address register. In response, the
					 * device clears a number of lowest-significant bits
					 * corresponding to the resource size. Finally, we write
					 * back the bar-address value as assigned by the BIOS.
					 */
					pci_config->write(bdf, bar_idx, ~0, Device::ACCESS_32BIT);
					unsigned const bar_size = pci_config->read(bdf, bar_idx, Device::ACCESS_32BIT);
					pci_config->write(bdf, bar_idx, bar_value, Device::ACCESS_32BIT);

					if (!Resource::Bar::mem64(bar_value)) {
						_resource[i] = Resource(bar_value, bar_size);
						++i;
					} else {
						/* also consume next BAR for MEM64 */
						unsigned const bar2_idx = bar_idx + 4;
						unsigned const bar2_value =
							pci_config->read(bdf, bar2_idx, Device::ACCESS_32BIT);
						pci_config->write(bdf, bar2_idx, ~0, Device::ACCESS_32BIT);
						unsigned const bar2_size =
							pci_config->read(bdf, bar2_idx, Device::ACCESS_32BIT);
						pci_config->write(bdf, bar2_idx, bar2_value, Device::ACCESS_32BIT);

						/* combine into first resource and mark second as invalid */
						_resource[i] = Resource(bar_value, bar_size,
						                        bar2_value, bar2_size);
						++i;
						_resource[i] = Resource();
						++i;
					}
				}
			}

			/**
			 * Accessor function for device location
			 */
			Pci::Bdf bdf() const { return _bdf; }

			void print(Output &out) const { Genode::print(out, bdf()); }

			/**
			 * Accessor functions for device information
			 */
			unsigned short device_id() { return _device_id; }
			unsigned short vendor_id() { return _vendor_id; }
			unsigned int  class_code() { return _class_code; }

			/**
			 * Return true if device is a PCI bridge
			 */
			bool pci_bridge() { return _header_type == HEADER_PCI_TO_PCI; }

			/**
			 * Return true if device is valid
			 */
			bool valid() { return _vendor_id != INVALID_VENDOR; }

			/**
			 * Return resource description by resource ID
			 */
			Platform::Pci::Resource resource(int resource_id)
			{
				/* return invalid resource if sanity check fails */
				if (!_resource_id_is_valid(resource_id))
					return Platform::Pci::Resource();

				return _resource[resource_id];
			}

			/**
			 * Read configuration space
			 */
			enum { DONT_TRACK_ACCESS = false };
			unsigned read(Config_access &pci_config, unsigned char address,
			              Device::Access_size size, bool track = true)
			{
				return pci_config.read(_bdf, address, size, track);
			}

			/**
			 * Write configuration space
			 */
			void write(Config_access &pci_config, unsigned char address,
			           unsigned long value, Device::Access_size size,
			           bool track = true)
			{
				pci_config.write(_bdf, address, value, size, track);
			}

			bool reg_in_use(Config_access &pci_config, unsigned char address,
			                Device::Access_size size) {
				return pci_config.reg_in_use(address, size); }

			void disable_bus_master_dma(Config_access &pci_config)
			{
				Pci_header header (pci_config, _bdf);

				if (header.read<Pci_header::Command::Dma>())
					header.write<Pci_header::Command::Dma>(0);
			}

			Device_bars save_bars()
			{
				Device_bars bars (_bdf);

				for (unsigned r = 0; r < Device::NUM_RESOURCES; r++) {
					if (!_resource_id_is_valid(r))
						break;

					bars.bar_addr[r] = _resource[r].base();
				}

				return bars;
			};

			void restore_bars(Config_access &config, Device_bars const &bars)
			{
				for (unsigned r = 0; r < Device::NUM_RESOURCES; r++) {
					if (!_resource_id_is_valid(r))
						break;

					/* index of base-address register in configuration space */
					unsigned const bar_idx = 0x10 + 4 * r;

					/* PCI protocol to write address after requesting size */
					config.write(_bdf, bar_idx, ~0U, Device::ACCESS_32BIT);
					config.read (_bdf, bar_idx, Device::ACCESS_32BIT);
					config.write(_bdf, bar_idx, bars.bar_addr[r],
					             Device::ACCESS_32BIT);
				}
			}
	};

	class Config_space : private List<Config_space>::Element
	{
		private:

			friend class List<Config_space>;

			uint32_t _bdf_start;
			uint32_t _func_count;
			addr_t   _base;

		public:

			using List<Config_space>::Element::next;

			Config_space(uint32_t bdf_start, uint32_t func_count, addr_t base)
			:
				_bdf_start(bdf_start), _func_count(func_count), _base(base) {}

			addr_t lookup_config_space(Pci::Bdf const bdf)
			{
				if ((_bdf_start <= bdf.value()) && (bdf.value() <= _bdf_start + _func_count - 1))
					return _base + (unsigned(bdf.value()) << 12);
				return 0;
			}
	};
}

#endif /* _X86__PCI_DEVICE_CONFIG_H_ */
