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
#include <base/log.h>
#include <base/capability.h>
#include <rm_session/connection.h>

/* L4lx includes */
#include <dataspace.h>
#include <env.h>

namespace Fiasco {
#include <l4/re/c/mem_alloc.h>
}

using namespace Fiasco;

extern "C" {

	long l4re_ma_alloc(unsigned long size, l4re_ds_t const mem,
	                   unsigned long flags)
	{
		using namespace L4lx;

		Dataspace *ds;
		if (Genode::log2(size) >= Chunked_dataspace::CHUNK_SIZE_LOG2) {
			ds = new (Genode::env()->heap())
				Chunked_dataspace("lx_memory", size, mem);
		} else {
			Genode::Dataspace_capability cap =
				Genode::env()->ram_session()->alloc(size);
			ds = new (Genode::env()->heap())
				Single_dataspace("lx_memory", size, cap, mem);
		}
		Env::env()->dataspaces()->insert(ds);
		return 0;
	}


	long l4re_ma_alloc_align(unsigned long size, l4re_ds_t const mem,
	                         unsigned long flags, unsigned long align) {
		return l4re_ma_alloc(size, mem, flags); }


	long l4re_ma_free(l4re_ds_t const mem)
	{
		Genode::warning(__func__, " not implemented");
		return 0;
	}

}
