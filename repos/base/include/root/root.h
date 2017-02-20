/*
 * \brief  Root interface
 * \author Norman Feske
 * \date   2006-05-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__ROOT__ROOT_H_
#define _INCLUDE__ROOT__ROOT_H_

#include <base/exception.h>
#include <base/rpc.h>
#include <base/rpc_args.h>
#include <base/affinity.h>
#include <session/capability.h>

namespace Genode {

	struct Root;
	template <typename> struct Typed_root;
}


struct Genode::Root
{
	/*********************
	 ** Exception types **
	 *********************/

	class Exception      : public ::Genode::Exception { };
	class Unavailable    : public Exception { };
	class Quota_exceeded : public Exception { };
	class Invalid_args   : public Exception { };

	typedef Rpc_in_buffer<160> Session_args;
	typedef Rpc_in_buffer<160> Upgrade_args;

	virtual ~Root() { }

	/**
	 * Create session
	 *
	 * \throw Unavailable
	 * \throw Quota_exceeded
	 * \throw Invalid_args
	 *
	 * \return capability to new session
	 */
	virtual Session_capability session(Session_args const &args,
	                                   Affinity     const &affinity) = 0;

	/**
	 * Extend resource donation to an existing session
	 */
	virtual void upgrade(Session_capability session, Upgrade_args const &args) = 0;

	/**
	 * Close session
	 */
	virtual void close(Session_capability session) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC_THROW(Rpc_session, Session_capability, session,
	                 GENODE_TYPE_LIST(Unavailable, Quota_exceeded, Invalid_args),
	                 Session_args const &, Affinity const &);
	GENODE_RPC_THROW(Rpc_upgrade, void, upgrade,
	                 GENODE_TYPE_LIST(Invalid_args),
	                 Session_capability, Upgrade_args const &);
	GENODE_RPC(Rpc_close, void, close, Session_capability);

	GENODE_RPC_INTERFACE(Rpc_session, Rpc_upgrade, Rpc_close);
};


/**
 * Root interface supplemented with information about the managed
 * session type
 *
 * This class template is used to automatically propagate the
 * correct session type to 'Parent::announce()' when announcing
 * a service.
 */
template <typename SESSION_TYPE>
struct Genode::Typed_root : Root
{
	typedef SESSION_TYPE Session_type;
};

#endif /* _INCLUDE__ROOT__ROOT_H_ */
