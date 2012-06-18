/*
 * \brief  Fiasco-specific part of RM-session implementation
 * \author Stefan Kalkowski
 * \date   2011-01-18
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <rm_session_component.h>
#include <map_local.h>

using namespace Genode;

void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	// TODO unmap it only from target space
	unmap_local(core_local_base, size >> get_page_size_log2());
}
