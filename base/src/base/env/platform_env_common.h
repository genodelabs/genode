/*
 * \brief  Platform environment of Genode process
 * \author Norman Feske
 * \date   2013-09-25
 *
 * Parts of 'Platform_env' shared accross all base platforms.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_ENV_COMMON_H_
#define _PLATFORM_ENV_COMMON_H_

#include <base/env.h>
#include <parent/client.h>
#include <ram_session/client.h>
#include <rm_session/client.h>
#include <cpu_session/client.h>
#include <pd_session/client.h>

namespace Genode {
	class Expanding_rm_session_client;
	class Expanding_ram_session_client;
	class Expanding_cpu_session_client;
}


/**
 * Repeatedly try to execute a function 'func'
 *
 * If the function 'func' throws an exception of type 'EXC', the 'handler'
 * is called and the function call is retried.
 */
template <typename EXC, typename FUNC, typename HANDLER>
auto retry(FUNC func, HANDLER handler) -> decltype(func())
{
	for (;;)
		try { return func(); }
		catch (EXC) { handler(); }
}


/**
 * Client object for a session that may get its session quota upgraded
 */
template <typename CLIENT>
struct Upgradeable_client : CLIENT
{
	typedef Genode::Capability<typename CLIENT::Rpc_interface> Capability;

	Capability _cap;

	Upgradeable_client(Capability cap) : CLIENT(cap), _cap(cap) { }

	void upgrade_ram(Genode::size_t quota)
	{
		PINF("upgrading quota donation for Env::%s (%zd bytes)",
		     CLIENT::Rpc_interface::service_name(), quota);

		char buf[128];
		Genode::snprintf(buf, sizeof(buf), "ram_quota=%zd", quota);
		Genode::env()->parent()->upgrade(_cap, buf);
	}
};


struct Genode::Expanding_ram_session_client : Upgradeable_client<Genode::Ram_session_client>
{
	Expanding_ram_session_client(Ram_session_capability cap)
	: Upgradeable_client<Genode::Ram_session_client>(cap) { }

	Ram_dataspace_capability alloc(size_t size, bool cached)
	{
		return retry<Ram_session::Out_of_metadata>(
			[&] () { return Ram_session_client::alloc(size, cached); },
			[&] () { upgrade_ram(8*1024); });
	}
};

#endif /* _PLATFORM_ENV_COMMON_H_ */
