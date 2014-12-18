/*
 * \brief  Process-local thread-context area
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <rm_session/connection.h>
#include <base/env.h>
#include <base/thread.h>
#include <platform_env_common.h>

using namespace Genode;


struct Expanding_rm_connection : Connection<Rm_session>, Expanding_rm_session_client
{
	/**
	 * Constructor
	 *
	 * \param start start of the managed VM-region
	 * \param size  size of the VM-region to manage
	 */
	Expanding_rm_connection(addr_t start = ~0UL, size_t size = 0) :
		Connection<Rm_session>(
			session("ram_quota=64K, start=0x%p, size=0x%zx",
			        start, size)),
		Expanding_rm_session_client(cap()) { }
};


struct Context_area_rm_session : Expanding_rm_connection
{
	Context_area_rm_session()
	: Expanding_rm_connection(0, Native_config::context_area_virtual_size())
	{
		addr_t local_base = Native_config::context_area_virtual_base();
		size_t size       = Native_config::context_area_virtual_size();

		env()->rm_session()->attach_at(dataspace(), local_base, size);
	}
};


namespace Genode {

	Rm_session *env_context_area_rm_session()
	{
		static Context_area_rm_session inst;
		return &inst;
	}

	Ram_session *env_context_area_ram_session()
	{
		return env()->ram_session();
	}
}

