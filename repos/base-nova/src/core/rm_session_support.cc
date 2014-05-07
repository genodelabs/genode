/*
 * \brief  RM-session implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <rm_session_component.h>
#include <nova_util.h>

using namespace Genode;

void Rm_client::unmap(addr_t core_local_base, addr_t, size_t size)
{
	using namespace Nova;

	Utcb * utcb = reinterpret_cast<Utcb *>(Genode::Thread_base::myself()->utcb());

	unmap_local(utcb, trunc_page(core_local_base),
	            (round_page(core_local_base + size) -
	            trunc_page(core_local_base)) / get_page_size(), false);
}
