/*
 * \brief  Client-side timer session interface
 * \author Norman Feske
 * \date   2006-05-31
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TIMER_SESSION__CLIENT_H_
#define _INCLUDE__TIMER_SESSION__CLIENT_H_

#include <timer_session/capability.h>
#include <base/rpc_client.h>

namespace Timer {

	struct Session_client : Genode::Rpc_client<Session>
	{
		explicit Session_client(Session_capability session)
		: Genode::Rpc_client<Session>(session) { }

		void msleep(unsigned ms) { call<Rpc_msleep>(ms); }

		unsigned long elapsed_ms() const { return call<Rpc_elapsed_ms>(); }
	};
}

#endif /* _INCLUDE__TIMER_SESSION__CLIENT_H_ */
