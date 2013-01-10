/*
 * \brief  Genode C API memory functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-07
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>

/* local includes */
#include <oklx_memory_maps.h>
#include <oklx_threads.h>

extern "C" {

	namespace Okl4 {
#include <genode/memory.h>
#include <l4/kdebug.h>
	}


	void *genode_malloc(unsigned long sz)
	{
		using namespace Genode;

		try {

			/* Get a new dataspace and attach it to our address space */
			Dataspace_capability ds = env()->ram_session()->alloc(sz);
			void *base = env()->rm_session()->attach(ds);

			/* Put the dataspace area in our database */
			Memory_area::memory_map()->insert(new (env()->heap())
			                                  Memory_area((addr_t)base,sz,ds));
			return base;
		}
		catch(...)
		{
			PWRN("Could not open dataspace!");
			return 0;
		}
	}


	void genode_set_pager()
	{
		Genode::Oklx_process::set_pager();
	}


	unsigned long genode_quota()
	{
		return Genode::env()->ram_session()->quota();
	}


	unsigned long genode_used_mem()
	{
		return Genode::env()->ram_session()->used();
	}


} // extern "C"
