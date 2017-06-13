/*
 * \brief  Client-side HW specific PD session interface
 * \author Stefan Kalkowski
 * \date   2017-06-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__HW_NATIVE_PD__CLIENT_H_
#define _INCLUDE__HW_NATIVE_PD__CLIENT_H_

#include <hw_native_pd/hw_native_pd.h>
#include <base/rpc_client.h>

namespace Genode { struct Hw_native_pd_client; }


struct Genode::Hw_native_pd_client : Rpc_client<Hw_native_pd>
{
	explicit Hw_native_pd_client(Capability<Native_pd> cap)
	: Rpc_client<Hw_native_pd>(static_cap_cast<Hw_native_pd>(cap)) { }

	void upgrade_cap_slab() override {
		call<Rpc_upgrade_cap_slab>(); }
};

#endif /* _INCLUDE__HW_NATIVE_PD__CLIENT_H_ */
