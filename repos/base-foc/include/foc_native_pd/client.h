/*
 * \brief  Client-side Fiasco.OC specific PD session interface
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2011-04-14
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC_NATIVE_PD__CLIENT_H_
#define _INCLUDE__FOC_NATIVE_PD__CLIENT_H_

#include <foc_native_pd/foc_native_pd.h>
#include <base/rpc_client.h>

namespace Genode { struct Foc_native_pd_client; }


struct Genode::Foc_native_pd_client : Rpc_client<Foc_native_pd>
{
	explicit Foc_native_pd_client(Capability<Native_pd> cap)
	: Rpc_client<Foc_native_pd>(static_cap_cast<Foc_native_pd>(cap)) { }

	Native_capability task_cap() { return call<Rpc_task_cap>(); }
};

#endif /* _INCLUDE__FOC_NATIVE_PD__CLIENT_H_ */
