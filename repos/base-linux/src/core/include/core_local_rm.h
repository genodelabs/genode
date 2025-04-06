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

/* Genode includes */
#include <pd_session/pd_session.h>

/* base-internal includes */
#include <base/internal/region_map_mmap.h>

/* core includes */
#include <types.h>

namespace Core { class Core_local_rm; }


struct Core::Core_local_rm : private Region_map_mmap, Pd_local_rm
{
	static void init(Rpc_entrypoint &);

	Core_local_rm(Rpc_entrypoint &ep)
	:
		Region_map_mmap(false),
		Pd_local_rm(*static_cast<Region_map_mmap *>(this))
	{
		init(ep);
	}
};

#endif /* _CORE__INCLUDE__CORE_REGION_MAP_H_ */
