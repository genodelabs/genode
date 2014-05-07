/*
 * \brief  Fiasco.OC specific PD session extension
 * \author Stefan Kalkowski
 * \date   2011-04-14
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FOC_PD_SESSION__FOC_PD_SESSION_H_
#define _INCLUDE__FOC_PD_SESSION__FOC_PD_SESSION_H_

#include <base/capability.h>
#include <base/rpc.h>
#include <pd_session/pd_session.h>

namespace Genode {

	struct Foc_pd_session : Pd_session
	{
		virtual ~Foc_pd_session() { }

		virtual Native_capability task_cap() = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_task_cap, Native_capability, task_cap);

		GENODE_RPC_INTERFACE_INHERIT(Pd_session, Rpc_task_cap);
	};
}

#endif /* _INCLUDE__FOC_PD_SESSION__FOC_PD_SESSION_H_ */
