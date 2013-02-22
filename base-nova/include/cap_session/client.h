/*
 * \brief  Client-side CAP session interface
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2006-07-10
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CAP_SESSION__CLIENT_H_
#define _INCLUDE__CAP_SESSION__CLIENT_H_

#include <cap_session/capability.h>
#include <cap_session/cap_session.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Cap_session_client : Rpc_client<Cap_session>
	{
		explicit Cap_session_client(Cap_session_capability session)
		: Rpc_client<Cap_session>(session) { }

		Native_capability alloc(Native_capability ep, addr_t entry = 0,
		                        addr_t flags = 0)
		{
			Native_capability cap = call<Rpc_alloc>(ep, entry, flags);

			using namespace Nova;

			/* set our local name */
			if (NOVA_OK != pt_ctrl(cap.local_name(), cap.local_name()))
				nova_die();
			/* disable the feature for security reasons now */
			revoke(Obj_crd(cap.local_name(), 0, Obj_crd::RIGHT_PT_CTRL));

			return cap;
		}

		void free(Native_capability cap) { call<Rpc_free>(cap); }
	};
}

#endif /* _INCLUDE__CAP_SESSION__CLIENT_H_ */
