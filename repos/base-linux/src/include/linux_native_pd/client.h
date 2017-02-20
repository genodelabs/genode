/*
 * \brief  Client-side of the Linux-specific PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LINUX_NATIVE_PD__CLIENT_H_
#define _INCLUDE__LINUX_NATIVE_PD__CLIENT_H_

#include <linux_native_pd/linux_native_pd.h>
#include <base/rpc_client.h>

namespace Genode { struct Linux_native_pd_client; }

struct Genode::Linux_native_pd_client : Rpc_client<Linux_native_pd>
{
	explicit Linux_native_pd_client(Capability<Native_pd> cap)
	: Rpc_client<Linux_native_pd>(static_cap_cast<Linux_native_pd>(cap)) { }

	void start(Capability<Dataspace> binary) {
		call<Rpc_start>(binary); }
};

#endif /* _INCLUDE__LINUX_NATIVE_PD__CLIENT_H_ */
