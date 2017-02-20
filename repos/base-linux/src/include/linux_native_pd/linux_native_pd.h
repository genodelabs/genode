/*
 * \brief  Linux-specific extension of the PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LINUX_NATIVE_PD__LINUX_NATIVE_PD_H_
#define _INCLUDE__LINUX_NATIVE_PD__LINUX_NATIVE_PD_H_

#include <pd_session/pd_session.h>
#include <dataspace/dataspace.h>

namespace Genode { struct Linux_native_pd; }


struct Genode::Linux_native_pd : Pd_session::Native_pd
{
	void start(Capability<Dataspace> binary);

	GENODE_RPC(Rpc_start, void, start, Capability<Dataspace>);
	GENODE_RPC_INTERFACE(Rpc_start);
};

#endif /* _INCLUDE__LINUX_NATIVE_PD__LINUX_NATIVE_PD_H_ */
