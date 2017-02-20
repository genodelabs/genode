/*
 * \brief  Client-side stub for the NOVA-specific PD session interface
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA_NATIVE_PD__CLIENT_H_
#define _INCLUDE__NOVA_NATIVE_PD__CLIENT_H_

#include <nova_native_pd/nova_native_pd.h>
#include <base/rpc_client.h>

namespace Genode { struct Nova_native_pd_client; }


struct Genode::Nova_native_pd_client : Rpc_client<Nova_native_pd>
{
	explicit Nova_native_pd_client(Capability<Native_pd> cap)
	: Rpc_client<Nova_native_pd>(static_cap_cast<Nova_native_pd>(cap)) { }

	Native_capability alloc_rpc_cap(Native_capability ep,
	                                addr_t entry, addr_t mtd) override
	{
		return call<Rpc_alloc_rpc_cap>(ep, entry, mtd);
	}

	void imprint_rpc_cap(Native_capability cap, unsigned long badge) override
	{
		call<Rpc_imprint_rpc_cap>(cap, badge);
	}
};

#endif /* _INCLUDE__NOVA_NATIVE_PD__CLIENT_H_ */
