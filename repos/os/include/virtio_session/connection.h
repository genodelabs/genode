/*
 * \brief  Connection to VirtIO bus service
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <util/retry.h>
#include <virtio_session/client.h>
#include <base/connection.h>

namespace Virtio { class Connection; }

struct Virtio::Connection : Genode::Connection<Virtio::Session>,
                            Virtio::Session_client
{
	enum { RAM_QUOTA = 1024UL, CAP_QUOTA = 10 };

	/**
	 * Constructor
	 */
	Connection(Genode::Env &env)
	: Genode::Connection<Virtio::Session>(env, session(env.parent(),
	                                      "ram_quota=%u, cap_quota=%u",
	                                      RAM_QUOTA, CAP_QUOTA)),
	  Session_client(cap()) { }

	template <typename FUNC>
	auto with_upgrade(FUNC func) -> decltype(func())
	{
		return Genode::retry<Genode::Out_of_ram>(
			[&] () {
				return Genode::retry<Genode::Out_of_caps>(
					[&] () { return func(); },
					[&] () { this->upgrade_caps(CAP_QUOTA); });
			},
			[&] () { this->upgrade_ram(RAM_QUOTA); }
		);
	}
};
