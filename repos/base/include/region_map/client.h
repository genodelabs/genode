/*
 * \brief  Client-side stub for region map
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-07-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGION_MAP__CLIENT_H_
#define _INCLUDE__REGION_MAP__CLIENT_H_

#include <base/rpc_client.h>
#include <region_map/region_map.h>

namespace Genode { class Region_map_client; }


class Genode::Region_map_client : public Rpc_client<Region_map>
{
	private:

		/*
		 * Multiple calls to get the dataspace capability on NOVA lead to the
		 * situation that the caller gets each time a new mapping of the same
		 * capability at different indices. But the client/caller assumes to
		 * get every time the very same index, e.g., in Noux the index is used
		 * to look up data structures attached to the capability. Therefore, we
		 * cache the dataspace capability on the first request.
		 *
		 * On all other base platforms, this member variable remains unused.
		 */
		Dataspace_capability _rm_ds_cap;

	public:

		explicit Region_map_client(Capability<Region_map>);

		Local_addr attach(Dataspace_capability ds, size_t size = 0,
		                  off_t offset = 0, bool use_local_addr = false,
		                  Local_addr local_addr = (void *)0,
		                  bool executable = false) override;

		void                 detach(Local_addr)                       override;
		void                 fault_handler(Signal_context_capability) override;
		State                state()                                  override;
		Dataspace_capability dataspace()                              override;
};

#endif /* _INCLUDE__REGION_MAP__CLIENT_H_ */
