/*
 * \brief  Process-local thread-context area
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <rm_session/connection.h>
#include <base/env.h>
#include <base/thread.h>


struct Context_area_rm_session : Genode::Rm_connection
{
	Context_area_rm_session()
	: Genode::Rm_connection(0, Genode::Native_config::context_area_virtual_size())
	{
		using namespace Genode;

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

