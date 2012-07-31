/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <base/stdint.h>
#include <util/touch.h>

#include <env.h>

namespace Fiasco {
#include <l4/re/c/rm.h>
#include <l4/sys/err.h>
#include <l4/sys/kdebug.h>
}

using namespace Fiasco;

static const bool DEBUG      = false; /* print usage of region map functions */
static const bool DEBUG_FIND = false; /* print also usage of region map lookups */

enum {
	L4RE_SEARCH_FOR_REGION = 0x20,
	L4RE_REGION_RESERVED   = 0x08
};

extern "C" {

	int l4re_rm_find(l4_addr_t *addr, unsigned long *size, l4_addr_t *offset,
	                 unsigned *flags, l4re_ds_t *m)
	{
		using namespace L4lx;

		if(DEBUG_FIND)
			PDBG("addr=%lx size=%lx", *addr, *size);

		*m = L4_INVALID_CAP;
		Region *r = Env::env()->rm()->find_region(addr, (Genode::size_t*)size);
		if (r) {
			if (r->ds())
				*m = r->ds()->ref();
			*flags = L4RE_REGION_RESERVED;
		} else
			*flags = 0;

		if(DEBUG_FIND)
			PDBG("Found addr=%lx size=%lx reserved?=%x ds=%lx",
		   	     *addr, *size, *flags, *m);
		return 0;
	}


	int l4re_rm_attach(void **start, unsigned long size, unsigned long flags,
	                   l4re_ds_t const mem, l4_addr_t offs,
	                   unsigned char align)
	{
		using namespace Genode;

		if (DEBUG)
			PDBG("start=%p size=%lx flags=%lx mem=%lx offs=%lx align=%x",
			     *start, size, flags, mem, offs, align);

		L4lx::Dataspace *ds = L4lx::Env::env()->dataspaces()->find_by_ref(mem);
		if (!ds) {
			PERR("mem=%lx doesn't exist", mem);
			return -L4_ERANGE;
		}

		if(!L4lx::Env::env()->rm()->attach_at(ds, size, offs, *start)) {
			if (flags & L4RE_SEARCH_FOR_REGION) /* search flag */
				*start = L4lx::Env::env()->rm()->attach(ds);
			else {
				PWRN("Couldn't attach ds of size %lx at %p", size, *start);
				return -L4_ERANGE;
			}
		}

		if (DEBUG)
			PDBG("attached at %p", *start);
		return 0;
	}


	int l4re_rm_detach(void *addr)
	{
		if(DEBUG)
			PDBG("addr=%p", addr);

		Genode::addr_t start = (Genode::addr_t) addr;
		Genode::size_t size  = 0;

		L4lx::Region *r = L4lx::Env::env()->rm()->find_region(&start, &size);
		if (!r) {
			PWRN("Nothing found at %p", addr);
			return -1;
		}

		Genode::env()->rm_session()->detach(addr);
		L4lx::Env::env()->rm()->free((void*)start);
		return 0;
	}


	int l4re_rm_reserve_area(l4_addr_t *start, unsigned long size,
	                         unsigned flags, unsigned char align)
	{
		if (DEBUG)
			PDBG("*start=%lx size=%lx align=%x flags=%x",
			     *start, size, align, flags);

		L4lx::Region *r = L4lx::Env::env()->rm()->reserve_range(size, align, *start);
		if (r) {
			*start = (l4_addr_t) r->addr();

			if (DEBUG)
				PDBG("return %lx", *start);

			return 0;
		}
		PWRN("Could not reserve area!");
		return -1;
	}


	int l4re_rm_free_area(l4_addr_t addr)
	{
		L4lx::Region* md = L4lx::Env::env()->rm()->metadata((void*)addr);

		if (DEBUG)
			PDBG("%lx", addr);

		if (!md) {
			PWRN("No region found at %p", (void*) addr);
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
