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
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CAP_SESSION__CAP_SESSION_H_
#define _INCLUDE__CAP_SESSION__CAP_SESSION_H_

#include <base/exception.h>
#include <base/native_types.h>
#include <session/session.h>

namespace Genode { struct Cap_session; }


struct Genode::Cap_session : Session
{
	class Out_of_metadata : public Exception { };

	static const char *service_name() { return "CAP"; }

	virtual ~Cap_session() { }

	/**
	 * Allocate new unique userland capability
	 *
	 * \param ep  entry point that will use this capability
	 *
	 * \throw Out_of_metadata  if meta-data backing store is exhausted
	 *
	 * \return new userland capability
	 */
	virtual Native_capability alloc(Native_capability ep) = 0;

	/**
	 * Free userland capability
	 *
	 * \param cap  userland capability to free
	 */
	virtual void free(Native_capability cap) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC_THROW(Rpc_alloc, Native_capability, alloc,
	                 GENODE_TYPE_LIST(Out_of_metadata), Native_capability);
	GENODE_RPC(Rpc_free, void, free, Native_capability);
	GENODE_RPC_INTERFACE(Rpc_alloc, Rpc_free);
};

#endif /* _INCLUDE__CAP_SESSION__CAP_SESSION_H_ */
