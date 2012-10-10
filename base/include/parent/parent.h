/*
 * \brief  Parent interface
 * \author Norman Feske
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PARENT__PARENT_H_
#define _INCLUDE__PARENT__PARENT_H_

#include <base/exception.h>
#include <base/rpc.h>
#include <base/rpc_args.h>
#include <base/thread.h>
#include <session/capability.h>
#include <root/capability.h>

namespace Genode {

	struct Parent
	{
		/*********************
		 ** Exception types **
		 *********************/

		class Exception      : public ::Genode::Exception { };
		class Service_denied : public Exception { };
		class Quota_exceeded : public Exception { };
		class Unavailable    : public Exception { };

		typedef Rpc_in_buffer<64>  Service_name;
		typedef Rpc_in_buffer<160> Session_args;
		typedef Rpc_in_buffer<160> Upgrade_args;


		virtual ~Parent() { }

		/**
		 * Tell parent to exit the program
		 */
		virtual void exit(int exit_value) = 0;

		/**
		 * Announce service to the parent
		 */
		virtual void announce(Service_name const &service_name,
		                      Root_capability service_root) = 0;

		/**
		 * Announce service to the parent
		 *
		 * \param service_root  root capability
		 *
		 * The type of the specified 'service_root' capability match with
		 * an interface that provides a 'Session_type' type (i.e., a
		 * 'Typed_root' interface). This 'Session_type' is expected to
		 * host a static function called 'service_name' returning the
		 * name of the provided interface as null-terminated string.
		 */
		template <typename ROOT_INTERFACE>
		void announce(Capability<ROOT_INTERFACE> const &service_root)
		{
			announce(ROOT_INTERFACE::Session_type::service_name(), service_root);
		}

		/**
		 * Create session to a service
		 *
		 * \param service_name     name of the requested interface
		 * \param args             session constructor arguments
		 *
		 * \throw Service_denied   parent denies session request
		 * \throw Quota_exceeded   our own quota does not suffice for
		 *                         the creation of the new session
		 * \throw Unavailable
		 *
		 * \return                 untyped capability to new session
		 *
		 * The use of this function is discouraged. Please use the type safe
		 * 'session()' template instead.
		 */
		virtual Session_capability session(Service_name const &service_name,
		                                   Session_args const &args) = 0;

		/**
		 * Create session to a service
		 *
		 * \param SESSION_TYPE  session interface type
		 * \param args          session constructor arguments
		 *
		 * \throw Service_denied   parent denies session request
		 * \throw Quota_exceeded   our own quota does not suffice for
		 *                         the creation of the new session
		 * \throw Unavailable
		 *
		 * \return                 capability to new session
		 */
		template <typename SESSION_TYPE>
		Capability<SESSION_TYPE> session(Session_args const &args)
		{
			Session_capability cap = session(SESSION_TYPE::service_name(), args);
			return reinterpret_cap_cast<SESSION_TYPE>(cap);
		}

		/**
		 * Transfer our quota to the server that provides the specified session
		 *
		 * \param to_session recipient session
		 * \param args       description of the amount of quota to transfer
		 *
		 * \throw Quota_exceeded  quota could not be transferred
		 *
		 * The 'args' argument has the same principle format as the 'args'
		 * argument of the 'session' function.
		 * The error case indicates that there is not enough unused quota on
		 * the source side.
		 */
		virtual void upgrade(Session_capability to_session,
		                     Upgrade_args const &args) = 0;

		/**
		 * Close session
		 */
		virtual void close(Session_capability session) = 0;

		/**
		 * Provide thread_cap of main thread
		 */
		virtual Thread_capability main_thread_cap() const = 0;

		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_exit, void, exit, int);
		GENODE_RPC(Rpc_announce, void, announce,
		           Service_name const &, Root_capability);
		GENODE_RPC_THROW(Rpc_session, Session_capability, session,
		                 GENODE_TYPE_LIST(Service_denied, Quota_exceeded, Unavailable),
		                 Service_name const &, Session_args const &);
		GENODE_RPC_THROW(Rpc_upgrade, void, upgrade,
		                 GENODE_TYPE_LIST(Quota_exceeded),
		                 Session_capability, Upgrade_args const &);
		GENODE_RPC(Rpc_close, void, close, Session_capability);
		GENODE_RPC(Rpc_main_thread, Thread_capability, main_thread_cap);

		GENODE_RPC_INTERFACE(Rpc_exit, Rpc_announce, Rpc_session, Rpc_upgrade,
		                     Rpc_close, Rpc_main_thread);
	};
}

#endif /* _INCLUDE__PARENT__PARENT_H_ */
