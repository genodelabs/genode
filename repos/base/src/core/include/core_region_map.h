/*
 * \brief  Core-specific region map
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_REGION_MAP_H_
#define _CORE__INCLUDE__CORE_REGION_MAP_H_

/* Genode includes */
#include <region_map/region_map.h>
#include <base/rpc_server.h>

/* core includes */
#include <dataspace_component.h>

namespace Genode { class Core_region_map; }


class Genode::Core_region_map : public Region_map
{
	private:

		Rpc_entrypoint &_ep;

	public:

		Core_region_map(Rpc_entrypoint &ep) : _ep(ep) { }

		Local_addr attach(Dataspace_capability, size_t size = 0,
		                  off_t offset=0, bool use_local_addr = false,
		                  Local_addr local_addr = 0,
		                  bool executable = false,
		                  bool writeable = true) override;

		void detach(Local_addr) override;

		void  fault_handler (Signal_context_capability) override { }
		State state         ()                          override { return State(); }

		Dataspace_capability dataspace() override { return Dataspace_capability(); }
};

#endif /* _CORE__INCLUDE__CORE_REGION_MAP_H_ */
