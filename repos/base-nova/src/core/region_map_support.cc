/*
 * \brief  Region map implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <rm_session_component.h>

using namespace Genode;


/***************
 ** Rm_client **
 ***************/

void Rm_client::unmap(addr_t, addr_t virt_base, size_t size)
{
	Locked_ptr<Address_space> locked_address_space(_address_space);

	if (locked_address_space.valid())
		locked_address_space->flush(virt_base, size);
}
