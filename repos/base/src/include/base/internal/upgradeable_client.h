/*
 * \brief  Utility for using a dynamically upgradeable session
 * \author Norman Feske
 * \date   2013-09-25
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__UPGRADEABLE_CLIENT_H_
#define _INCLUDE__BASE__INTERNAL__UPGRADEABLE_CLIENT_H_

#include <base/env.h>
#include <base/log.h>

namespace Genode { template <typename> struct Upgradeable_client; }


/**
 * Client object for a session that may get its session quota upgraded
 */
template <typename CLIENT>
struct Genode::Upgradeable_client : CLIENT
{
	typedef Genode::Capability<typename CLIENT::Rpc_interface> Capability;

	Parent::Client::Id _id;

	Upgradeable_client(Capability cap, Parent::Client::Id id)
	: CLIENT(cap), _id(id) { }

	void upgrade_ram(size_t quota)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "ram_quota=%lu", quota);

		env_deprecated()->parent()->upgrade(_id, buf);
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__UPGRADEABLE_CLIENT_H_ */
