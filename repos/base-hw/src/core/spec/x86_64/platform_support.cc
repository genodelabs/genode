/*
 * \brief   Platform implementations specific for x86_64
 * \author  Reto Buerki
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <pic.h>
#include <kernel/kernel.h>
#include <multiboot.h>

using namespace Genode;

/* contains physical pointer to multiboot */
extern "C" Genode::addr_t __initial_bx;

void Platform::_init_additional() { };

Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_LAPIC_BASE,  Board::MMIO_LAPIC_SIZE  },
		{ Board::MMIO_IOAPIC_BASE, Board::MMIO_IOAPIC_SIZE },
		{ 0, 0}
	};

	unsigned const max = sizeof(_regions) / sizeof(_regions[0]);

	if (!_regions[max - 1].size)
		_regions[max - 1] = { __initial_bx & ~0xFFFUL, get_page_size() };

	return i < max ? &_regions[i] : nullptr;
}


void Platform::setup_irq_mode(unsigned irq_number, unsigned trigger,
                              unsigned polarity)
{
	Kernel::pic()->ioapic.setup_irq_mode(irq_number, trigger, polarity);
}


bool Platform::get_msi_params(const addr_t mmconf, addr_t &address,
                              addr_t &data, unsigned &irq_number)
{
	return false;
}


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[16];

	Multiboot_info::Mmap v = Genode::Multiboot_info(__initial_bx).phys_ram(i);
	if (!v.base)
		return nullptr;

	Multiboot_info::Mmap::Addr::access_t base = v.read<Multiboot_info::Mmap::Addr>();
	Multiboot_info::Mmap::Length::access_t size = v.read<Multiboot_info::Mmap::Length>();

	unsigned const max = sizeof(_regions) / sizeof(_regions[0]);

	if (i < max && _regions[i].size == 0) {
		if (base == 0 && size >= get_page_size()) {
			/*
			 * Exclude first physical page, so that it will become part of the
			 * MMIO allocator. The framebuffer requests this page as MMIO.
			 */
			base  = get_page_size();
			size -= get_page_size();
		}
		_regions[i] = { base, size };
	} else if (i >= max)
		warning("physical ram region ", (void*)base, "+", (size_t)size,
		        " will be not used");

	return i < max ? &_regions[i] : nullptr;
}
