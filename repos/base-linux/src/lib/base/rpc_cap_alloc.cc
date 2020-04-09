/*
 * \brief  Back end of the RPC entrypoint
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/rpc_server.h>
#include <deprecated/env.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

using namespace Genode;


Native_capability Rpc_entrypoint::_alloc_rpc_cap(Pd_session& pd, Native_capability,
                                                 addr_t)
{
	/* first we allocate a cap from core, to allow accounting of caps. */
	for (;;) {
		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		try { pd.alloc_rpc_cap(_cap); break; }
		catch (Out_of_ram)  { ram_upgrade = Ram_quota { 2*1024*sizeof(long) }; }
		catch (Out_of_caps) { cap_upgrade = Cap_quota { 4 }; }

		env_deprecated()->parent()->upgrade(Parent::Env::pd(),
		                                    String<100>("ram_quota=", ram_upgrade, ", "
		                                                "cap_quota=", cap_upgrade).string());
	}

	return Thread::native_thread().epoll.alloc_rpc_cap();
}


void Rpc_entrypoint::_free_rpc_cap(Pd_session& pd, Native_capability cap)
{
	Native_thread::Epoll &epoll = Thread::native_thread().epoll;

	/*
	 * Flag RPC entrypoint as exited to prevent 'free_rpc_cap' from issuing
	 * a remote control request.
	 */
	if (_exit_handler.exit)
		epoll.rpc_ep_exited();

	/*
	 * Perform the accounting of the PDs cap quota at core, to remain
	 * consistent with other kernel platforms.
	 */
	pd.free_rpc_cap(Native_capability());

	epoll.free_rpc_cap(cap);
}
