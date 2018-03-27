/*
 * \brief   Platform implementations specific for x86_64_muen
 * \author  Reto Buerki
 * \author  Adrian-Ken Rueegsegger
 * \date    2015-04-21
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>
#include <platform.h>
#include <sinfo_instance.h>

#include <base/internal/unmanaged_singleton.h>

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

struct Msi_address : Register<32>
{
	enum { BASE = 0xfee00010 };

	struct Bits_0 : Bitfield<5, 15> { };
	struct Bits_1 : Bitfield<2, 1> { };
	struct Handle : Bitset_2<Bits_0, Bits_1> { };

	/**
	 * Return MSI address register value for given handle to enable Interrupt
	 * Requests in Remappable Format, see VT-d specification section 5.1.2.2.
	 */
	static access_t to_msi_addr(Msi_address::Handle::access_t const handle)
	{
		access_t addr = BASE;
		Handle::set(addr, handle);
		return addr;
	}
};


void Platform::setup_irq_mode(unsigned, unsigned, unsigned) { }


bool Platform::get_msi_params(const addr_t mmconf, addr_t &address,
                              addr_t &data, unsigned &irq_number)
{
	const unsigned sid = Mmconf_address::to_sid(mmconf);
	const struct Sinfo::Device_type *dev = sinfo()->get_device(sid);

	if (!dev) {
		error("error retrieving Muen info for device with SID ", Hex(sid));
		return false;
	}
	if (!dev->ir_count) {
		error("device ", Hex(sid), " has no IRQ assigned");
		return false;
	}
	if (!(dev->flags & Sinfo::DEV_MSI_FLAG)) {
		error("device ", Hex(sid), " not configured for MSI");
		return false;
	}

	data = 0;
	address = Msi_address::to_msi_addr(dev->irte_start);
	irq_number = dev->irq_start;

	log("enabling MSI for device with SID ", Hex(sid), ": "
	    "IRTE ", dev->irte_start, ", IRQ ", irq_number);
	return true;
}


void Platform::_init_additional()
{
	/* export subject info page as ROM module */
	_rom_fs.insert(new (core_mem_alloc())
	               Rom_module((addr_t)Sinfo::PHYSICAL_BASE_ADDR,
	               Sinfo::SIZE, "subject_info_page"));
}


enum { COM1_PORT = 0x3f8 };
Board::Serial::Serial(Genode::addr_t, Genode::size_t, unsigned baudrate)
:X86_uart(COM1_PORT, 0, baudrate) {}
