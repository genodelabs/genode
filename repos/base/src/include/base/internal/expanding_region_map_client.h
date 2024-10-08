/*
 * \brief  Region-map client that upgrades PD-session quota on demand
 * \author Norman Feske
 * \date   2013-09-25
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__EXPANDING_REGION_MAP_CLIENT_H_
#define _INCLUDE__BASE__INTERNAL__EXPANDING_REGION_MAP_CLIENT_H_

/* Genode includes */
#include <util/retry.h>
#include <region_map/client.h>
#include <pd_session/client.h>

/* base-internal includes */
#include <base/internal/upgradeable_client.h>

namespace Genode { class Expanding_region_map_client; }


struct Genode::Expanding_region_map_client : Region_map_client
{
	Upgradeable_client<Genode::Pd_session_client> _pd_client;

	Expanding_region_map_client(Parent &parent, Pd_session_capability pd,
	                            Capability<Region_map> rm,
	                            Parent::Client::Id pd_id)
	: Region_map_client(rm), _pd_client(parent, pd, pd_id) { }

	Attach_result attach(Dataspace_capability ds, Attr const &attr) override
	{
		for (;;) {
			Attach_result const result = Region_map_client::attach(ds, attr);
			if      (result == Attach_error::OUT_OF_RAM)  _pd_client.upgrade_ram(8*1024);
			else if (result == Attach_error::OUT_OF_CAPS) _pd_client.upgrade_caps(2);
			else
				return result;
		}
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__EXPANDING_REGION_MAP_CLIENT_H__ */
