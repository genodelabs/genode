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
#include <base/printf.h>
#include <dataspace/client.h>

/* L4lx includes */
#include <env.h>

namespace Fiasco {
#include <l4/re/c/dataspace.h>
#include <l4/sys/err.h>
#include <l4/sys/kdebug.h>
}

using namespace Fiasco;

static bool DEBUG = false;

extern "C" {

	int l4re_ds_map_region(const l4re_ds_t ds, l4_addr_t offset, unsigned long flags,
	                       l4_addr_t min_addr, l4_addr_t max_addr)
	{
		using namespace L4lx;

		if(DEBUG)
			PDBG("ds=%lx offset=%lx flags=%lx min_addr=%lx max_addr=%lx",
			     ds, offset, flags, min_addr, max_addr);

		Dataspace *ref = Env::env()->dataspaces()->find_by_ref(ds);
		if (!ref) {
			PWRN("ds=%lx doesn't exist", ds);
			enter_kdebug("ENOTF");
			return L4_ERANGE;
		}

		try {
			Genode::env()->rm_session()->attach_at(ref->cap(), min_addr);
		} catch(...) {
			PWRN("Could not attach dataspace %s at %p!", ref->name(), (void*)min_addr);
			enter_kdebug("EXC");
			return -1;
		}
		return 0;
	}


	long l4re_ds_size(const l4re_ds_t ds)
	{
		using namespace L4lx;

		if (DEBUG)
			PDBG("ds=%lx", ds);

		Dataspace *ref = Env::env()->dataspaces()->find_by_ref(ds);
		if (!ref) {
			PWRN("ds=%lx doesn't exist", ds);
			return -1;
		}

		return ref->size();
	}


	int l4re_ds_phys(const l4re_ds_t ds, l4_addr_t offset,
	                 l4_addr_t *phys_addr, l4_size_t *phys_size)
	{
		using namespace L4lx;

		if (DEBUG)
			PDBG("ds=%lx offset=%lx", ds, offset);

		Dataspace *ref = Env::env()->dataspaces()->find_by_ref(ds);
		if (!ref) {
			PWRN("ds=%lx doesn't exist", ds);
			enter_kdebug("ERR");
			return -1;
		}

		if (DEBUG)
			PDBG("Found dataspace %s", ref->name());

		if (!ref->cap().valid()) {
			PWRN("Cannot determine physical address for dataspace %s!", ref->name());
			return -1;
		}

		Genode::Dataspace_client dsc(ref->cap());
		*phys_addr = dsc.phys_addr() + offset;
		*phys_size = dsc.size() - offset;
		return 0;
	}


	int l4re_ds_copy_in(const l4re_ds_t ds, l4_addr_t dst_offs, const l4re_ds_t src,
	                    l4_addr_t src_offs, unsigned long size)
	{
		PWRN("%s: Not implemented yet!",__func__);
		return 0;
	}


	int l4re_ds_info(const l4re_ds_t ds, l4re_ds_stats_t *stats)
	{
		using namespace L4lx;

		if (DEBUG)
			PDBG("ds=%lx", ds);

		Dataspace *ref = Env::env()->dataspaces()->find_by_ref(ds);
		if (!ref) {
			PWRN("ds=%lx doesn't exist", ds);
			return -1;
		}

		if (DEBUG)
			PDBG("Found dataspace %s", ref->name());

		stats->size = ref->size();
		return 0;
	}

}
