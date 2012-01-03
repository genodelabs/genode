/*
 * \brief  Server-side timer session interface
 * \author Norman Feske
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TIMER_SESSION__SERVER_H_
#define _INCLUDE__TIMER_SESSION__SERVER_H_

#include <timer_session/timer_session.h>
#include <base/rpc_server.h>

namespace Timer {

	struct Session_server : Genode::Rpc_object<Session> { };
}

#endif /* _INCLUDE__TIMER_SESSION__SERVER_H_ */
