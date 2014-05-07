/*
 * \brief  Input session interface
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INPUT_SESSION__INPUT_SESSION_H_
#define _INCLUDE__INPUT_SESSION__INPUT_SESSION_H_

#include <dataspace/capability.h>
#include <base/rpc_server.h>
#include <session/session.h>

namespace Input {

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Input"; }

		virtual ~Session() { }

		/**
		 * Return capability to event buffer dataspace
		 */
		virtual Genode::Dataspace_capability dataspace() = 0;

		/**
		 * Request input state
		 *
		 * \return  true if new events are available
		 */
		virtual bool is_pending() const = 0;

		/**
		 * Flush pending events to event buffer
		 *
		 * \return  number of flushed events
		 */
		virtual int flush() = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
		GENODE_RPC(Rpc_is_pending, bool, is_pending);
		GENODE_RPC(Rpc_flush, int, flush);

		GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_is_pending, Rpc_flush);
	};
}

#endif /* _INCLUDE__INPUT_SESSION__INPUT_SESSION_H_ */
