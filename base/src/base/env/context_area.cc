/*
 * \brief  Process-local thread-context area
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
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
	: Genode::Rm_connection(0, Genode::Thread_base::CONTEXT_AREA_VIRTUAL_SIZE)
	{
		using namespace Genode;

		addr_t local_base = Thread_base::CONTEXT_AREA_VIRTUAL_BASE;
		size_t size       = Thread_base::CONTEXT_AREA_VIRTUAL_SIZE;

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

