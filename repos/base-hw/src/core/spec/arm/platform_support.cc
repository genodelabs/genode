/*
 * \brief   ARM specific platform implementations
 * \author  Adrian-Ken Rueegsegger
 * \date    2015-03-18
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>

using namespace Genode;

void Platform::_init_io_port_alloc() { };

void Platform::_init_additional() { };

void Platform::setup_irq_mode(unsigned, unsigned, unsigned) { }

Native_region * mmio_regions(unsigned);


void Platform::_init_io_mem_alloc()
{
	Native_region * r = mmio_regions(0);
	for (unsigned i = 0; r; r = mmio_regions(++i))
		_io_mem_alloc.add_range(r->base, r->size);

	r = mmio_regions(0);
	for (unsigned i = 0; r; r = mmio_regions(++i))
		_core_mem_alloc.virt_alloc()->remove_range(r->base, r->size);
};


long Platform::irq(long const user_irq) { return user_irq; }


bool Platform::get_msi_params(const addr_t mmconf, addr_t &address,
                              addr_t &data, unsigned &irq_number)
{
	return false;
}
