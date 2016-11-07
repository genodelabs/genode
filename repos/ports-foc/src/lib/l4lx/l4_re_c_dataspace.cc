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
#include <dataspace/client.h>

/* L4lx includes */
#include <env.h>

namespace Fiasco {
#include <l4/re/c/dataspace.h>
#include <l4/sys/err.h>
#include <l4/sys/kdebug.h>
}

using namespace Fiasco;

extern "C" {

	int l4re_ds_map_region(const l4re_ds_t ds, l4_addr_t offset, unsigned long flags,
	                       l4_addr_t min_addr, l4_addr_t max_addr)
	{
		using namespace L4lx;

		Dataspace *ref = Env::env()->dataspaces()->find_by_ref(ds);
		if (!ref) {
			Genode::warning(__func__, ": ds=", Genode::Hex(ds), " doesn't exist");
			enter_kdebug("ENOTF");
			return L4_ERANGE;
		}

		try {
			Genode::env()->rm_session()->attach_at(ref->cap(), min_addr);
		} catch(...) {
			Genode::warning(__func__, ": could not attach "
			                "dataspace ", ref->name(), " at ", (void*)min_addr);
			enter_kdebug("EXC");
			return -1;
		}
		return 0;
	}


	long l4re_ds_size(const l4re_ds_t ds)
	{
		using namespace L4lx;

		Dataspace *ref = Env::env()->dataspaces()->find_by_ref(ds);
		if (!ref) {
			Genode::warning(__func__, ": ds=", Genode::Hex(ds), " doesn't exist");
			return -1;
		}

		return ref->size();
	}


	int l4re_ds_phys(const l4re_ds_t ds, l4_addr_t offset,
	                 l4_addr_t *phys_addr, l4_size_t *phys_size)
	{
		using namespace L4lx;

		Dataspace *ref = Env::env()->dataspaces()->find_by_ref(ds);
		if (!ref) {
			Genode::warning(__func__, ": ds=", Genode::Hex(ds), " doesn't exist");
			enter_kdebug("ERR");
			return -1;
		}

		if (!ref->cap().valid()) {
			Genode::warning(__func__, ": cannot determine physical address for "
			                "dataspace ", ref->name());
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
		Genode::warning(__func__, ": not implemented");
		return 0;
	}


	int l4re_ds_info(const l4re_ds_t ds, l4re_ds_stats_t *stats)
	{
		using namespace L4lx;

		Dataspace *ref = Env::env()->dataspaces()->find_by_ref(ds);
		if (!ref) {
			Genode::warning(__func__, ": ds=", Genode::Hex(ds), " doesn't exist");
			return -1;
		}

		stats->size = ref->size();
		return 0;
	}

}
