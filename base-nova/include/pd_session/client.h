/*
 * \brief  Client-side pd session interface
 * \author Christian Helmuth
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
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
		explicit Pd_session_client(Pd_session_capability const &session)
		: Rpc_client<Pd_session>(session) { }

		int bind_thread(Thread_capability const &thread) {
			return call<Rpc_bind_thread>(thread); }

		int assign_parent(Parent_capability const &parent)
		{
			Parent_capability par = parent;
			par.solely_map();
			return call<Rpc_assign_parent>(par);
		}

		bool assign_pci(addr_t pci_config_memory_address) {
			return call<Rpc_assign_pci>(pci_config_memory_address); }
	};
}

#endif /* _INCLUDE__PD_SESSION__CLIENT_H_ */
