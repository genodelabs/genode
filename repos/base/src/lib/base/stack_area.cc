/*
 * \brief  Component-local stack area
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <region_map/region_map.h>
#include <ram_session/ram_session.h>

/* base-internal includes */
#include <base/internal/globals.h>

namespace Genode {
	Region_map  *env_stack_area_region_map;
	Ram_session *env_stack_area_ram_session;
}

