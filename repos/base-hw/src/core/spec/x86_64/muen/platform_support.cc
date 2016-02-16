/*
 * \brief   Platform implementations specific for x86_64_muen
 * \author  Reto Buerki
 * \author  Adrian-Ken Rueegsegger
 * \date    2015-04-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <platform.h>
#include <sinfo.h>

using namespace Genode;

struct Mmconf_address : Register<64>
{
	enum { PCI_CONFIG_BASE = 0xf8000000 };

	struct Sid : Bitfield<12, 16> { };

	/**
	 * Calculate SID (source-id, see VT-d spec section 3.4.1) from device PCI
	 * config space address.
	 */
	static unsigned to_sid(access_t const addr) {
		return Sid::get(addr - PCI_CONFIG_BASE); }
};

struct Msi_handle : Register<16>
{
	struct Bits_0 : Bitfield<0, 15> { };
	struct Bits_1 : Bitfield<15, 1> { };
};

struct Msi_address : Register<32>
{
	enum { BASE = 0xfee00010 };

	struct Bits_0 : Bitfield<5, 15> { };
	struct Bits_1 : Bitfield<2, 1> { };

	/**
	 * Return MSI address register value for given handle to enable Interrupt
	 * Requests in Remappable Format, see VT-d specification section 5.1.2.2.
	 */
	static access_t to_msi_addr(Msi_handle::access_t const handle)
	{
		access_t addr = BASE;
		Bits_0::set(addr, Msi_handle::Bits_0::get(handle));
		Bits_1::set(addr, Msi_handle::Bits_1::get(handle));
		return addr;
	}
};


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* Sinfo pages */
		{ 0x000e00000000, 0x7000 },

		/* Timer page */
		{ 0x000e00010000, 0x1000 },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


void Platform::setup_irq_mode(unsigned, unsigned, unsigned) { }


bool Platform::get_msi_params(const addr_t mmconf, addr_t &address,
                              addr_t &data, unsigned &irq_number)
{
	const unsigned sid = Mmconf_address::to_sid(mmconf);

	struct Sinfo::Dev_info dev_info;
	if (!Sinfo::get_dev_info(sid, &dev_info)) {
		PERR("error retrieving Muen info for device with SID 0x%x", sid);
		return false;
	}
	if (!dev_info.msi_capable) {
		PERR("device 0x%x not configured for MSI", sid);
		return false;
	}

	data = 0;
	address = Msi_address::to_msi_addr(dev_info.irte_start);
	irq_number = dev_info.irq_start;

	PDBG("enabling MSI for device with SID 0x%x: IRTE %d, IRQ %d",
		 sid, dev_info.irte_start, irq_number);
	return true;
}


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 25*1024*1024, 256*1024*1024 }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}
