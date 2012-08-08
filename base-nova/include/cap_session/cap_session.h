/*
 * \brief  CAP-session interface
 * \author Norman Feske
 * \date   2006-06-23
 *
 * A 'Cap_session' is an allocator of user-level capabilities.
 * User-level capabilities are used to reference server objects
 * across address spaces.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CAP_SESSION__CAP_SESSION_H_
#define _INCLUDE__CAP_SESSION__CAP_SESSION_H_

#include <base/native_types.h>
#include <session/session.h>

namespace Genode {

	struct Cap_session : Session
	{
		static const char *service_name() { return "CAP"; }

		virtual ~Cap_session() { }

		/**
		 * Allocate new unique userland capability
		 *
		 * \param ep  entry point that will use this capability
		 *
		 * \return new userland capability
		 */
		virtual Native_capability alloc(Native_capability ep,
		                                addr_t entry = 0,
		                                addr_t flags = 0) = 0;

		/**
		 * Free userland capability
		 *
		 * \param cap  userland capability to free
		 */
		virtual void free(Native_capability cap) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_alloc, Native_capability, alloc,
		           Native_capability, addr_t, addr_t);
		GENODE_RPC(Rpc_free, void, free, Native_capability);
		GENODE_RPC_INTERFACE(Rpc_alloc, Rpc_free);
	};
}

#endif /* _INCLUDE__CAP_SESSION__CAP_SESSION_H_ */
