/*
 * \brief  Component-local implementation of a PD session
 * \author Norman Feske
 * \date   2016-04-29
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCAL_PD_SESSION_H_
#define _INCLUDE__BASE__INTERNAL__LOCAL_PD_SESSION_H_

/* Genode includes */
#include <pd_session/client.h>
#include <linux_native_cpu/client.h>

/* base-internal includes */
#include <base/internal/local_capability.h>
#include <base/internal/region_map_mmap.h>
#include <base/internal/stack_area.h>

namespace Genode { struct Local_pd_session; }


struct Genode::Local_pd_session : Pd_session_client
{
	Region_map_mmap _address_space { false };
	Region_map_mmap _stack_area    { true,  stack_area_virtual_size() };
	Region_map_mmap _linker_area   { true, Pd_session::LINKER_AREA_SIZE };

	Local_pd_session(Pd_session_capability pd) : Pd_session_client(pd) { }

	Capability<Region_map> address_space()
	{
		return Local_capability<Region_map>::local_cap(&_address_space);
	}

	Capability<Region_map> stack_area()
	{
		return Local_capability<Region_map>::local_cap(&_stack_area);
	}

	Capability<Region_map> linker_area()
	{
		return Local_capability<Region_map>::local_cap(&_linker_area);
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__LOCAL_PD_SESSION_H_ */
