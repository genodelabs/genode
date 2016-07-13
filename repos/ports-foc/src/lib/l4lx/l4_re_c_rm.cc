/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <base/stdint.h>
#include <util/touch.h>

#include <env.h>

namespace Fiasco {
#include <l4/re/c/rm.h>
#include <l4/sys/err.h>
#include <l4/sys/kdebug.h>
}

using namespace Fiasco;

enum {
	L4RE_SEARCH_FOR_REGION = 0x20,
	L4RE_REGION_RESERVED   = 0x08
};

extern "C" {

	int l4re_rm_find(l4_addr_t *addr, unsigned long *size, l4_addr_t *offset,
	                 unsigned *flags, l4re_ds_t *m)
	{
		using namespace L4lx;

		*m = L4_INVALID_CAP;
		Region *r = Env::env()->rm()->find_region(addr, (Genode::size_t*)size);
		if (r) {
			if (r->ds())
				*m = r->ds()->ref();
			*flags = L4RE_REGION_RESERVED;
		} else
			*flags = 0;

		return 0;
	}


	int l4re_rm_attach(void **start, unsigned long size, unsigned long flags,
	                   l4re_ds_t const mem, l4_addr_t offs,
	                   unsigned char align)
	{
		using namespace Genode;

		void *original_start = *start;

		L4lx::Dataspace *ds = L4lx::Env::env()->dataspaces()->find_by_ref(mem);
		if (!ds) {
			error(__func__, "mem=", Hex(mem), " doesn't exist");
			return -L4_ERANGE;
		}

		while (!L4lx::Env::env()->rm()->attach_at(ds, size, offs, *start)) {
			if (flags & L4RE_SEARCH_FOR_REGION) /* search flag */ {
				/* the original start address might have a different alignment */
				l4_addr_t start_addr = (l4_addr_t)*start;
				l4_addr_t aligned_start_addr = align_addr(start_addr, align);
				if (aligned_start_addr != start_addr) {
					*start = (void*)aligned_start_addr;
				} else {
					if (start_addr <= ((addr_t)~0 - 2*(1 << align) + 1)) {
						start_addr += (1 << align);
						*start = (void*)start_addr;
					} else {
						warning(__func__, ": couldn't attach ds of "
						        "size ", Hex(size), " at ", original_start);
						return -L4_ERANGE;
					}
				}
			} else {
				warning(__func__, ": couldn't attach ds of "
				        "size ", Hex(size), " at ", original_start);
				return -L4_ERANGE;
			}
		}

		return 0;
	}


	int l4re_rm_detach(void *addr)
	{
		Genode::addr_t start = (Genode::addr_t) addr;
		Genode::size_t size  = 0;

		L4lx::Region *r = L4lx::Env::env()->rm()->find_region(&start, &size);
		if (!r) {
			Genode::warning(__func__, ": nothing found at ", addr);
			return -1;
		}

		Genode::env()->rm_session()->detach(addr);
		L4lx::Env::env()->rm()->free((void*)start);
		return 0;
	}


	int l4re_rm_reserve_area(l4_addr_t *start, unsigned long size,
	                         unsigned flags, unsigned char align)
	{
		L4lx::Region *r = L4lx::Env::env()->rm()->reserve_range(size, align, *start);
		if (r) {
			*start = (l4_addr_t) r->addr();

			return 0;
		}
		Genode::warning(__func__, ": could not reserve area!");
		return -1;
	}


	int l4re_rm_free_area(l4_addr_t addr)
	{
		L4lx::Region* md = L4lx::Env::env()->rm()->metadata((void*)addr);

		if (!md) {
			Genode::warning(__func__, ": no region found at ", Genode::Hex(addr));
			return -1;
		}

		/* gets freed only if there is no dataspace attached */
		if (!md->ds()->cap().valid())
			L4lx::Env::env()->rm()->free((void*)addr);

		return 0;
	}


	void l4re_rm_show_lists(void)
	{
		L4lx::Env::env()->rm()->dump();
	}

}
