/*
 * \brief  Connection to Platform service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <util/retry.h>
#include <base/connection.h>
#include <platform_session/client.h>

namespace Platform { struct Connection; }


struct Platform::Connection : Genode::Connection<Session>, Client
{
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env)
	:
		Genode::Connection<Session>(env, session(env.parent(),
		                                         "ram_quota=16K, cap_quota=%u",
		                                         CAP_QUOTA)),
		Client(cap())
	{ }

	template <typename FUNC>
	auto with_upgrade(FUNC func) -> decltype(func())
	{
		return Genode::retry<Genode::Out_of_ram>(
			[&] () {
				return Genode::retry<Genode::Out_of_caps>(
					[&] () { return func(); },
					[&] () { this->upgrade_caps(2); });
			},
			[&] () { this->upgrade_ram(4096); }
		);
	}
};
