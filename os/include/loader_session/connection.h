/*
 * \brief  Connection to Loader service
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LOADER_SESSION__CONNECTION_H_
#define _INCLUDE__LOADER_SESSION__CONNECTION_H_

#include <loader_session/client.h>
#include <base/connection.h>

namespace Loader {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection(Genode::size_t ram_quota)
		:
			Genode::Connection<Session>(session("ram_quota=%zd", ram_quota)),
			Session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__LOADER_SESSION__CONNECTION_H_ */
