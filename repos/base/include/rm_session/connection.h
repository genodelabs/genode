/*
 * \brief  Connection to RM service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2023 Genode Labs GmbH
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


struct Genode::Rm_connection : Connection<Rm_session>
{
	Rm_session_client _client { cap() };

	Rm_connection(Env &env)
	:
		Connection<Rm_session>(env, {}, Ram_quota { 64*1024 }, Args())
	{ }

	/**
	 * Wrapper of 'create' that handles session-quota upgrades on demand
	 */
	Capability<Region_map> create(size_t size)
	{
		Capability<Region_map> result { };
		using Error = Rm_session::Create_error;
		while (!result.valid())
			_client.create(size).with_result(
				[&] (Capability<Region_map> cap) { result = cap; },
				[&] (Error e) {
					switch (e) {
					case Error::OUT_OF_RAM:  upgrade_ram(8*1024); break;
					case Error::OUT_OF_CAPS: upgrade_caps(2);     break;
					}
				});
		return result;
	}

	void destroy(Capability<Region_map> cap) { _client.destroy(cap); }
};

#endif /* _INCLUDE__RM_SESSION__CONNECTION_H_ */
