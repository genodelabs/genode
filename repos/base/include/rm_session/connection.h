/*
 * \brief  Connection to RM service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RM_SESSION__CONNECTION_H_
#define _INCLUDE__RM_SESSION__CONNECTION_H_

#include <rm_session/client.h>
#include <base/connection.h>
#include <util/retry.h>

namespace Genode { struct Rm_connection; }


struct Genode::Rm_connection : Connection<Rm_session>, Rm_session_client
{
	enum { RAM_QUOTA = 64*1024 };

	/**
	 * Constructor
	 */
	Rm_connection(Env &env)
	:
		Connection<Rm_session>(env, session(env.parent(), "ram_quota=%u, cap_quota=%u",
		                       RAM_QUOTA, CAP_QUOTA)),
		Rm_session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Rm_connection() __attribute__((deprecated))
	:
		Connection<Rm_session>(session("ram_quota=%u, cap_quota=%u", RAM_QUOTA, CAP_QUOTA)),
		Rm_session_client(cap())
	{ }

	/**
	 * Wrapper over 'create' that handles resource requests
	 * from the server.
	 */
	Capability<Region_map> create(size_t size) override
	{
		enum { UPGRADE_ATTEMPTS = 16U };

		return Genode::retry<Out_of_ram>(
			[&] () {
				return Genode::retry<Out_of_caps>(
					[&] () { return Rm_session_client::create(size); },
					[&] () { upgrade_caps(2); },
					UPGRADE_ATTEMPTS);
			},
			[&] () { upgrade_ram(8*1024); },
			UPGRADE_ATTEMPTS);
	}
};

#endif /* _INCLUDE__RM_SESSION__CONNECTION_H_ */
