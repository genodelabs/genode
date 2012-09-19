/*
 * \brief  Connection to timer service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TIMER_SESSION__CONNECTION_H_
#define _INCLUDE__TIMER_SESSION__CONNECTION_H_

#include <timer_session/client.h>
#include <base/connection.h>

namespace Timer {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection()
		:
			/*
			 * Because the timer service uses one thread per timer session,
			 * we donate two pages. One of them is used as stack for the
			 * timer thread and the other page holds the session meta data.
			 */
			Genode::Connection<Session>(session("ram_quota=%zd",
			                                    sizeof(Genode::addr_t)*2048)),

			Session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__TIMER_SESSION__CONNECTION_H_ */
