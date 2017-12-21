/*
 * \brief  Connection to Gpu service
 * \author Josef Soentgen
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_SESSION__CONNECTION_H_
#define _INCLUDE__GPU_SESSION__CONNECTION_H_

#include <gpu_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Gpu { struct Connection; }

struct Gpu::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Genode::Capability<Gpu::Session> _session(Genode::Parent &parent,
	                                          char const *label, Genode::size_t quota)
	{
		return session(parent, "ram_quota=%ld, label=\"%s\"", quota, label);
	}

	/**
	 * Constructor
	 *
	 * \param quota  initial amount of quota used for allocating Gpu
	 *               memory
	 */
	Connection(Genode::Env    &env,
	           Genode::size_t  quota = Session::REQUIRED_QUOTA,
	           const char     *label = "")
	:
		Genode::Connection<Session>(env, _session(env.parent(), label, quota)),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__GPU_SESSION__CONNECTION_H_ */
