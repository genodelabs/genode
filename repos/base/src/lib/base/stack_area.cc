/*
 * \brief  Component-local stack area
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <region_map/region_map.h>
#include <base/ram_allocator.h>

/* base-internal includes */
#include <base/internal/globals.h>

namespace Genode {
	Region_map    *env_stack_area_region_map;
	Ram_allocator *env_stack_area_ram_allocator;
}

