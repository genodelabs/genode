/*
 * \brief  Linux-specific extension of the PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LINUX_PD_SESSION__LINUX_PD_SESSION_H_
#define _INCLUDE__LINUX_PD_SESSION__LINUX_PD_SESSION_H_

#include <pd_session/pd_session.h>
#include <dataspace/dataspace.h>

namespace Genode {

	struct Linux_pd_session : Pd_session
	{
		void start(Capability<Dataspace> binary);


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_start, void, start, Capability<Dataspace>);
		GENODE_RPC_INTERFACE_INHERIT(Pd_session, Rpc_start);
	};
}

#endif /* _INCLUDE__LINUX_PD_SESSION__LINUX_PD_SESSION_H_ */
