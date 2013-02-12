/*
 * \brief  Client-side timer session interface
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2006-05-31
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Copyright (C) 2012 Intel Corporation    
 * 
 * Modifications are contributed under the terms and conditions of the
 * Genode Contributor's Agreement executed by Intel.
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

		void usleep(unsigned us) { call<Rpc_usleep>(us); }

		unsigned long elapsed_ms() const { return call<Rpc_elapsed_ms>(); }
	};
}

#endif /* _INCLUDE__TIMER_SESSION__CLIENT_H_ */
