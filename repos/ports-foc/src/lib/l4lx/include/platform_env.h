/*
 * \brief  Platform environment of Genode process
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-28
 *
 * This file is a generic variant of the platform environment, which is
 * suitable for platforms such as L4ka::Pistachio and L4/Fiasco. On other
 * platforms, it may be replaced by a platform-specific version residing
 * in the corresponding 'base-<platform>' repository.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_ENV_H_
#define _PLATFORM_ENV_H_

/* Genode includes */
#include <base/log.h>
#include <base/env.h>
#include <base/heap.h>
#include <rm_session/client.h>
#include <util/arg_string.h>

namespace Genode {
	struct Expanding_rm_session_client;
}


/**
 * Repeatedly try to execute a function 'func'
 *
 * If the function 'func' throws an exception of type 'EXC', the 'handler'
 * is called and the function call is retried.
 *
 * \param EXC       exception type to handle
 * \param func      functor to execute
 * \param handler   exception handler executed if 'func' raised an exception
 *                  of type 'EXC'
 * \param attempts  number of attempts to execute 'func' before giving up
 *                  and reflecting the exception 'EXC' to the caller. If not
 *                  specified, attempt infinitely.
 */
template <typename EXC, typename FUNC, typename HANDLER>
auto retry(FUNC func, HANDLER handler, unsigned attempts = ~0U) -> decltype(func())
{
	for (unsigned i = 0; attempts == ~0U || i < attempts; i++)
		try { return func(); }
		catch (EXC) { handler(); }

	throw EXC();
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
		Genode::log("upgrading quota donation for "
		            "Env::", CLIENT::Rpc_interface::service_name(), " "
		            "(", quota, " bytes)");

		char buf[128];
		Genode::snprintf(buf, sizeof(buf), "ram_quota=%zd", quota);

		Genode::env()->parent()->upgrade(_cap, buf);
	}
};

#endif /* _PLATFORM_ENV_H_ */
