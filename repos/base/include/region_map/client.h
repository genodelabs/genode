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
		Dataspace_capability _rm_ds_cap { };

	public:

		explicit Region_map_client(Capability<Region_map>);

		Attach_result        attach(Dataspace_capability, Attr const &) override;
		void                 detach(addr_t)                             override;
		void                 fault_handler(Signal_context_capability)   override;
		Fault                fault()                                    override;
		Dataspace_capability dataspace()                                override;
};

#endif /* _INCLUDE__REGION_MAP__CLIENT_H_ */
