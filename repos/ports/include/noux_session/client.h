/*
 * \brief  Noux-session client interface
 * \author Norman Feske
 * \date   2011-02-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOUX_SESSION__CLIENT_H_
#define _INCLUDE__NOUX_SESSION__CLIENT_H_

#include <noux_session/noux_session.h>
#include <noux_session/capability.h>
#include <base/rpc_client.h>
#include <base/log.h>

namespace Noux {

	struct Session_client : Rpc_client<Session>
	{
			explicit Session_client(Session_capability session)
			: Rpc_client<Session>(session) { }


			Dataspace_capability sysio_dataspace()
			{
				return call<Rpc_sysio_dataspace>();
			}

			bool syscall(Syscall sc)
			{
				static bool verbose = false;

				bool result = call<Rpc_syscall>(sc);

				if ((result == false) && verbose)
					error("syscall ", syscall_name(sc), " failed");

				return result;
			}

			int next_open_fd(int start_fd)
			{
				return call<Rpc_next_open_fd>(start_fd);
			}

			Capability<Region_map> lookup_region_map(addr_t const addr)
			{
				return call<Rpc_lookup_region_map>(addr);
			}
	};
}

#endif /* _INCLUDE__NOUX_SESSION__CLIENT_H_ */
