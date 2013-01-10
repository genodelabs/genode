/*
 * \brief  Noux connection
 * \author Norman Feske
 * \date   2011-02-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOUX_SESSION__CONNECTION_H_
#define _INCLUDE__NOUX_SESSION__CONNECTION_H_

#include <noux_session/client.h>
#include <base/connection.h>

namespace Noux {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection() :
			Genode::Connection<Session>(session("")),
			Session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__NOUX_SESSION__CONNECTION_H_ */
