/*
 * \brief  Attach stack area to local address space
 * \author Norman Feske
 * \date   2016-07-06
 *
 * This function resides in a distinct compilation unit because it is not
 * used for hybrid components where the Genode::Thread API is implemented
 * via POSIX threads.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/platform_env.h>

using namespace Genode;


void Platform_env::_attach_stack_area()
{
	_local_pd_session._address_space.attach_at(_local_pd_session._stack_area.dataspace(),
	                                           stack_area_virtual_base(),
	                                           stack_area_virtual_size());
}
