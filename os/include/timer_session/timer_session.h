/*
 * \brief  Timer session interface
 * \author Norman Feske
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TIMER_SESSION__TIMER_SESSION_H_
#define _INCLUDE__TIMER_SESSION__TIMER_SESSION_H_

#include <session/session.h>

namespace Timer {

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Timer"; }

		virtual ~Session() { }

		/**
		 * Sleep number of milliseconds
		 */
		virtual void msleep(unsigned ms) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_msleep, void, msleep, unsigned);

		GENODE_RPC_INTERFACE(Rpc_msleep);
	};
}

#endif /* _INCLUDE__TIMER_SESSION__TIMER_SESSION_H_ */
