/*
 * \brief  Functions not offered by Genode's DDE-kit
 * \author Sebastian Sumpf
 * \date   2010-10-21
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/printf.h>
#include <timer_session/connection.h>
#include <util/misc_math.h>

extern "C" {
#include <dde_kit/pci.h>
#include "dde_support.h"
}

using namespace Genode;

/***************************************************
 ** Support for aligned and DMA memory allocation **
 ***************************************************/

enum { BACKING_STORE_SIZE = 1024 * 1024 };


static Allocator_avl& allocator()
{
	static Allocator_avl _avl(env()->heap());
	return _avl;
}

extern "C" int dde_mem_init(int bus, int dev, int func)
{
	try {
		addr_t base = dde_kit_pci_alloc_dma_buffer(bus, dev, func,
		                                           BACKING_STORE_SIZE);
		/* add to allocator */
		allocator().add_range(base, BACKING_STORE_SIZE);
	} catch (...) {
		return false;
	}

	return true;
}

extern "C" void *dde_alloc_memblock(dde_kit_size_t size, dde_kit_size_t align,
                                    dde_kit_size_t offset)
{
	void *ptr;
	if (allocator().alloc_aligned(size, &ptr, log2(align)).is_error()) {
		PERR("memory allocation failed in alloc_memblock (size=%zd, align=%zx,"
		     " offset=%zx)", size, align, offset);
		return 0;
	}
	return ptr;
}


extern "C" void dde_free_memblock(void *p, dde_kit_size_t size)
{
	allocator().free(p, size);
}


/***********
 ** Timer **
 ***********/

extern "C" void dde_timer2_udelay(unsigned long usecs)
{
	/*
	 * This function is called only once during rdtsc calibration (usecs will be
	 * 10000, see dde.c 'udelay'. We do not use DDE timers here, since Genode's
	 * timer connection is the most precise one around.
	 */
	Timer::Connection timer;
	timer.usleep(usecs);
}
