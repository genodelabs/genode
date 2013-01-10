/*
 * \brief  Genode C API file functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
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
#include <rom_session/connection.h>
#include <dataspace/client.h>

/* OKLinux support includes */
#include <oklx_memory_maps.h>

extern "C" {

	namespace Okl4 {
#include <genode/open.h>
	}


	void* genode_open(const char *name, unsigned long *sz)
	{
		using namespace Genode;
		try {

			/* Open the file dataspace and attach it */
			Rom_connection rc(name);
			rc.on_destruction(Rom_connection::KEEP_OPEN);
			Rom_dataspace_capability cap = rc.dataspace();
			Dataspace_client dsc(cap);
			*sz = dsc.size();
			void *base = env()->rm_session()->attach(cap);

			/* Put it in our database */
			Memory_area::memory_map()->insert(new (env()->heap())
			                                  Memory_area((addr_t)base, *sz,
			                                              cap));
			return base;
		}
		catch(...)
		{
			PWRN("Could not open rom dataspace %s!", name);
			return 0;
		}
	}

} // extern "C"
