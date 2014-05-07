/*
 * \brief  Client-side Fiasco.OC specific PD session interface
 * \author Stefan Kalkowski
 * \date   2011-04-14
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FOC_PD_SESSION__CLIENT_H_
#define _INCLUDE__FOC_PD_SESSION__CLIENT_H_

#include <foc_pd_session/foc_pd_session.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Foc_pd_session_client : Rpc_client<Foc_pd_session>
	{
		explicit Foc_pd_session_client(Capability<Foc_pd_session> session)
		: Rpc_client<Foc_pd_session>(session) { }

		int bind_thread(Thread_capability thread) {
			return call<Rpc_bind_thread>(thread); }

		int assign_parent(Parent_capability parent) {
			return call<Rpc_assign_parent>(parent); }

		Native_capability task_cap() { return call<Rpc_task_cap>(); }
	};

}

#endif /* _INCLUDE__FOC_PD_SESSION__CLIENT_H_ */
