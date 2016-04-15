/*
 * \brief  OKL4-specific core-local region map
 * \author Norman Feske
 * \date   2009-04-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

		Rpc_entrypoint *_ds_ep;

	public:

		Core_region_map(Rpc_entrypoint *ds_ep): _ds_ep(ds_ep) { }

		Local_addr attach(Dataspace_capability ds_cap, size_t size=0,
		                  off_t offset=0, bool use_local_addr = false,
		                  Local_addr local_addr = 0,
		                  bool executable = false);

		void detach(Local_addr) { }

		Pager_capability add_client(Thread_capability thread) {
			return Pager_capability(); }

		void remove_client(Pager_capability) { }

		void fault_handler(Signal_context_capability handler) { }

		State state() { return State(); }

		Dataspace_capability dataspace() { return Dataspace_capability(); }
};

#endif /* _CORE__INCLUDE__CORE_REGION_MAP_H_ */
