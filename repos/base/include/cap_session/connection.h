/*
 * \brief  Connection to CAP service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CAP_SESSION__CONNECTION_H_
#define _INCLUDE__CAP_SESSION__CONNECTION_H_

#include <cap_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Cap_connection : Connection<Cap_session>, Cap_session_client
	{
		Cap_connection()
		:
			Connection<Cap_session>(session("ram_quota=4K")),
			Cap_session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__CAP_SESSION__CONNECTION_H_ */
