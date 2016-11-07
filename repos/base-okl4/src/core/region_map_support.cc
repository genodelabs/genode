/*
 * \brief  OKL4-specific part of region-map implementation
 * \author Norman Feske
 * \date   2009-04-10
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <region_map_component.h>

using namespace Genode;


void Rm_client::unmap(addr_t, addr_t virt_base, size_t size)
{
	Locked_ptr<Address_space> locked_address_space(_address_space);

	if (locked_address_space.valid())
		locked_address_space->flush(virt_base, size);
}
