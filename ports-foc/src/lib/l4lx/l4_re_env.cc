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
#include <base/printf.h>
#include <rom_session/connection.h>
#include <dataspace/client.h>

#include <env.h>

namespace Fiasco {
#include <l4/re/env.h>
#include <l4/sys/kdebug.h>
}

using namespace Fiasco;

static const bool DEBUG = false;
static l4re_env_t __l4re_env;

extern void* l4lx_kinfo;


extern "C" {

	l4re_env_cap_entry_t const * l4re_env_get_cap_l(char const *name,
	                                                unsigned l,
	                                                l4re_env_t const *e)
	{
		using namespace L4lx;

		if (DEBUG)
			PDBG("name=%s l=%x", name, l);

		try {
			Genode::Rom_connection rom(name);
			Genode::size_t size = Genode::Dataspace_client(rom.dataspace()).size();
			Genode::Dataspace_capability cap = Genode::env()->ram_session()->alloc(size);

			void *dst = L4lx::Env::env()->rm()->attach(cap, "initrd");
			void *src = Genode::env()->rm_session()->attach(rom.dataspace());

			Genode::memcpy(dst, src, size);
			Genode::env()->rm_session()->detach(src);

			l4re_env_cap_entry_t *entry = new (Genode::env()->heap())
				l4re_env_cap_entry_t();
			Region *r = Env::env()->rm()->find_region((Genode::addr_t*)&dst, &size);
			entry->cap = r->ds()->ref();
			return entry;
		} catch(Genode::Rom_connection::Rom_connection_failed) {
			PWRN("File %s is missing", name);
		}
		return 0;
	}


	l4_kernel_info_t *l4re_kip(void)
	{
		return (l4_kernel_info_t*) l4lx_kinfo;
	}


	l4re_env_t *l4re_env(void)
	{
		return &__l4re_env;
	}

}
