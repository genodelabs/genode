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

#include <base/internal/unmanaged_singleton.h>

using namespace Genode;

/* contains physical pointer to multiboot */
extern "C" Genode::addr_t __initial_bx;

void Platform::_init_additional() { };

Memory_region_array & Platform::core_mmio_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::MMIO_LAPIC_BASE,  Board::MMIO_LAPIC_SIZE  },
		Memory_region { Board::MMIO_IOAPIC_BASE, Board::MMIO_IOAPIC_SIZE },
		Memory_region { __initial_bx & ~0xFFFUL, get_page_size() });
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


Memory_region_array & Platform::ram_regions()
{
	static Memory_region_array ram;

	/* initialize memory pool */
	if (ram.count() == 0) {
		using Mmap = Multiboot_info::Mmap;

		for (unsigned i = 0; true; i++) {
			Mmap v = Multiboot_info(__initial_bx).phys_ram(i);
			if (!v.base) break;

			Mmap::Addr::access_t   base = v.read<Mmap::Addr>();
			Mmap::Length::access_t size = v.read<Mmap::Length>();

			/*
			 * Exclude first physical page, so that it will become part of the
			 * MMIO allocator. The framebuffer requests this page as MMIO.
			 */
			if (base == 0 && size >= get_page_size()) {
				base  = get_page_size();
				size -= get_page_size();
			}
			ram.add(Memory_region { base, size });
		}
	}

	return ram;
}
