/*
 * \brief  Utility for using a dynamically upgradeable session
 * \author Norman Feske
 * \date   2013-09-25
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

	Capability _cap;

	Upgradeable_client(Capability cap) : CLIENT(cap), _cap(cap) { }

	void upgrade_ram(size_t quota)
	{
		log("upgrading quota donation for Env::", CLIENT::Rpc_interface::service_name(),
		    " (", quota, " bytes)");

		char buf[128];
		snprintf(buf, sizeof(buf), "ram_quota=%zu", quota);

		env()->parent()->upgrade(_cap, buf);
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__UPGRADEABLE_CLIENT_H_ */
