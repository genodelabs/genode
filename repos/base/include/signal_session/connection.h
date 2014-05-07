/*
 * \brief  Connection to signal service
 * \author Norman Feske
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__CONNECTION_H_
#define _INCLUDE__SIGNAL_SESSION__CONNECTION_H_

#include <signal_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Signal_connection : Connection<Signal_session>, Signal_session_client
	{
		Signal_connection()
		:
			Connection<Signal_session>(session("ram_quota=12K")),
			Signal_session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__CAP_SESSION__CONNECTION_H_ */
