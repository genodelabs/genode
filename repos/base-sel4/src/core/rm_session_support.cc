/*
 * \brief  Kernel-specific supplements of the RM service
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <rm_session_component.h>

using namespace Genode;


void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	Locked_ptr<Address_space> locked_address_space(_address_space);

	if (locked_address_space.is_valid())
		locked_address_space->flush(virt_base, size);
}
