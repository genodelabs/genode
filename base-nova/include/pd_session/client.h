/*
 * \brief  Client-side pd session interface
 * \author Christian Helmuth
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PD_SESSION__CLIENT_H_
#define _INCLUDE__PD_SESSION__CLIENT_H_

#include <pd_session/capability.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Pd_session_client : Rpc_client<Pd_session>
	{
		explicit Pd_session_client(Pd_session_capability session)
		: Rpc_client<Pd_session>(session) { }

		int bind_thread(Thread_capability thread) {
			return call<Rpc_bind_thread>(thread); }

		int assign_parent(Parent_capability parent) {
			parent.solely_map();
			return call<Rpc_assign_parent>(parent); }
	};
}

#endif /* _INCLUDE__PD_SESSION__CLIENT_H_ */
