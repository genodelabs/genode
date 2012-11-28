/*
 * \brief  Functions not offered by Genode's DDE-kit
 * \author Sebastian Sumpf
 * \date   2010-10-21
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/printf.h>
#include <dataspace/client.h>
#include <timer_session/connection.h>
#include <util/misc_math.h>

extern "C" {
#include <dde_kit/pgtab.h>
}

using namespace Genode;

/*******************************************
 ** Support for aligned memory allocation **
 *******************************************/

enum { BACKING_STORE_SIZE = 1024 * 1024 };


Allocator_avl *allocator()
{
	static Allocator_avl _avl(env()->heap());
	return &_avl;
}


void __attribute__((constructor)) init()
{
	try {
		Dataspace_capability ds_cap = env()->ram_session()->alloc(BACKING_STORE_SIZE);
		addr_t base = (addr_t)env()->rm_session()->attach(ds_cap);

		/* add to allocator */
		allocator()->add_range(base, BACKING_STORE_SIZE);

		/* add to DDE-kit page tables */
		addr_t phys = Dataspace_client(ds_cap).phys_addr();
		dde_kit_pgtab_set_region_with_size((void *)base, phys, BACKING_STORE_SIZE);
	} catch (...) {
		PERR("Initialization of block memory failed!");
	}
}


extern "C" void *alloc_memblock(size_t size, size_t align)
{
	void *ptr;
	if (allocator()->alloc_aligned(size, &ptr, log2(align)).is_error()) {
		PERR("memory allocation failed in alloc_memblock");
		return 0;
	};
	return ptr;
}


extern "C" void free_memblock(void *p, size_t size)
{
	allocator()->free(p, size);
}


/***********
 ** Timer **
 ***********/

extern "C" void timer2_udelay(unsigned long usecs)
{
	/*
	 * This function is called only once during rdtsc calibration (usecs will be
	 * 10000, see dde.c 'udelay'. We do not use DDE timers here, since Genode's
	 * timer connection is the precised one around.
	 */
	Timer::Connection timer;
	timer.msleep(usecs / 1000);
}
