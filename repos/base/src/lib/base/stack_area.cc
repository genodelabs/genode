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

/* base-internal includes */
#include <base/internal/platform_env_common.h>
#include <base/internal/stack_area.h>

namespace Genode {
	Region_map  *env_stack_area_region_map;
	Ram_session *env_stack_area_ram_session;
}

