/*
 * \brief  Core-specific region map for Linux
 * \author Norman Feske
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_REGION_MAP_H_
#define _CORE__INCLUDE__CORE_REGION_MAP_H_

/* base-internal includes */
#include <base/internal/region_map_mmap.h>

namespace Genode { class Core_region_map; }


struct Genode::Core_region_map : Region_map_mmap
{
	Core_region_map(Rpc_entrypoint &ep) : Region_map_mmap(false) { }
};

#endif /* _CORE__INCLUDE__CORE_REGION_MAP_H_ */
