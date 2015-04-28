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


/**
 * Initialize I/O memory allocator with MMIO regions specified by platform.
 * Use byte granuarity for MMIO regions because on some platforms, devices
 * driven by core share a physical page with devices driven outside of
 * core. Using byte granuarlity allows handing out the MMIO page to trusted
 * user-level device drivers.
 */
void Platform::_init_io_mem_alloc()
{
	Native_region * r = _mmio_regions(0);
	for (unsigned i = 0; r; r = _mmio_regions(++i))
		_io_mem_alloc.add_range(r->base, r->size);
};


long Platform::irq(long const user_irq) { return user_irq; }
