/*
 * \brief  Parent interface
 * \author Norman Feske
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
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

namespace Genode { class Parent; }


class Genode::Parent
{
	private:

		/**
		 * Recursively announce inherited service interfaces
		 *
		 * At compile time, the 'ROOT' type is inspected for the presence
		 * of the 'Rpc_inherited_interface' type in the corresponding
		 * session interface. If present, the session type gets announced.
		 * This works recursively.
		 */
		template <typename ROOT>
		void _announce_base(Capability<ROOT> const &, Meta::Bool_to_type<false> *) { }

		/*
		 * This overload gets selected if the ROOT interface corresponds to
		 * an inherited session type.
		 */
		template <typename ROOT>
		inline void _announce_base(Capability<ROOT> const &, Meta::Bool_to_type<true> *);

	public:

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

		/**
		 * Use 'String' instead of 'Rpc_in_buffer' because 'Resource_args'
		 * is used as both in and out parameter.
		 */
		typedef String<160> Resource_args;


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
		 * host a class function called 'service_name' returning the
		 * name of the provided interface as null-terminated string.
		 */
		template <typename ROOT_INTERFACE>
		void announce(Capability<ROOT_INTERFACE> const &service_root)
		{
			typedef typename ROOT_INTERFACE::Session_type Session;
			announce(Session::service_name(), service_root);

			/*
			 * Announce inherited session types
			 *
			 * Select the overload based on the presence of the type
			 * 'Rpc_inherited_interface' within the session type.
			 */
			_announce_base(service_root,
			               (Meta::Bool_to_type<Rpc_interface_is_inherited<Session>::VALUE> *)0);
		}

		/**
		 * Create session to a service
		 *
		 * \param service_name     name of the requested interface
		 * \param args             session constructor arguments
		 * \param affinity         preferred CPU affinity for the session
		 *
		 * \throw Service_denied   parent denies session request
		 * \throw Quota_exceeded   our own quota does not suffice for
		 *                         the creation of the new session
		 * \throw Unavailable
		 *
		 * \return                 untyped capability to new session
		 *
		 * The use of this method is discouraged. Please use the type safe
		 * 'session()' template instead.
		 */
		virtual Session_capability session(Service_name const &service_name,
		                                   Session_args const &args,
		                                   Affinity     const &affinity = Affinity()) = 0;

		/**
		 * Create session to a service
		 *
		 * \param SESSION_TYPE     session interface type
		 * \param args             session constructor arguments
		 * \param affinity         preferred CPU affinity for the session
		 *
		 * \throw Service_denied   parent denies session request
		 * \throw Quota_exceeded   our own quota does not suffice for
		 *                         the creation of the new session
		 * \throw Unavailable
		 *
		 * \return                 capability to new session
		 */
		template <typename SESSION_TYPE>
		Capability<SESSION_TYPE> session(Session_args const &args,
		                                 Affinity     const &affinity = Affinity())
		{
			Session_capability cap = session(SESSION_TYPE::service_name(),
			                                 args, affinity);
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
		 * argument of the 'session' operation.
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

		/**
		 * Register signal handler for resource notifications
		 */
		virtual void resource_avail_sigh(Signal_context_capability sigh) = 0;

		/**
		 * Request additional resources
		 *
		 * By invoking this method, a process is able to inform its
		 * parent about the need for additional resources. The argument
		 * string contains a resource description in the same format as
		 * used for session-construction arguments. In particular, for
		 * requesting additional RAM quota, the argument looks like
		 * "ram_quota=<amount>" where 'amount' is the amount of additional
		 * resources expected from the parent. If the parent complies with
		 * the request, it submits a resource-available signal to the
		 * handler registered via 'resource_avail_sigh()'. On the reception
		 * of such a signal, the process can re-evaluate its resource quota
		 * and resume execution.
		 */
		virtual void resource_request(Resource_args const &args) = 0;

		/**
		 * Register signal handler for resource yield notifications
		 *
		 * Using the yield signal, the parent is able to inform the process
		 * about its wish to regain resources.
		 */
		virtual void yield_sigh(Signal_context_capability sigh) = 0;

		/**
		 * Obtain information about the amount of resources to free
		 *
		 * The amount of resources returned by this method is the
		 * goal set by the parent. It is not commanded but merely meant
		 * as a friendly beg to cooperate. The process is not obligated
		 * to comply. If the process decides to take action to free
		 * resources, it can inform its parent about the availability
		 * of freed up resources by calling 'yield_response()'.
		 */
		virtual Resource_args yield_request() = 0;

		/**
		 * Notify the parent about a response to a yield request
		 */
		virtual void yield_response() = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_exit, void, exit, int);
		GENODE_RPC(Rpc_announce, void, announce,
		           Service_name const &, Root_capability);
		GENODE_RPC_THROW(Rpc_session, Session_capability, session,
		                 GENODE_TYPE_LIST(Service_denied, Quota_exceeded, Unavailable),
		                 Service_name const &, Session_args const &, Affinity const &);
		GENODE_RPC_THROW(Rpc_upgrade, void, upgrade,
		                 GENODE_TYPE_LIST(Quota_exceeded),
		                 Session_capability, Upgrade_args const &);
		GENODE_RPC(Rpc_close, void, close, Session_capability);
		GENODE_RPC(Rpc_main_thread, Thread_capability, main_thread_cap);
		GENODE_RPC(Rpc_resource_avail_sigh, void, resource_avail_sigh,
		           Signal_context_capability);
		GENODE_RPC(Rpc_resource_request, void, resource_request,
		           Resource_args const &);
		GENODE_RPC(Rpc_yield_sigh, void, yield_sigh, Signal_context_capability);
		GENODE_RPC(Rpc_yield_request, Resource_args, yield_request);
		GENODE_RPC(Rpc_yield_response, void, yield_response);

		typedef Meta::Type_tuple<Rpc_exit,
		        Meta::Type_tuple<Rpc_announce,
		        Meta::Type_tuple<Rpc_session,
		        Meta::Type_tuple<Rpc_upgrade,
		        Meta::Type_tuple<Rpc_close,
		        Meta::Type_tuple<Rpc_main_thread,
		        Meta::Type_tuple<Rpc_resource_avail_sigh,
		        Meta::Type_tuple<Rpc_resource_request,
		        Meta::Type_tuple<Rpc_yield_sigh,
		        Meta::Type_tuple<Rpc_yield_request,
		        Meta::Type_tuple<Rpc_yield_response,
		                         Meta::Empty>
		        > > > > > > > > > > Rpc_functions;
};


template <typename ROOT_INTERFACE>
void
Genode::Parent::_announce_base(Genode::Capability<ROOT_INTERFACE> const &service_root,
                               Genode::Meta::Bool_to_type<true> *)
{
	/* shortcut for inherited session type */
	typedef typename ROOT_INTERFACE::Session_type::Rpc_inherited_interface
	        Session_type_inherited;

	/* shortcut for root interface type matching the inherited session type */
	typedef Typed_root<Session_type_inherited> Root_inherited;

	/* convert root capability to match the inherited session type */
	Capability<Root>           root           = service_root;
	Capability<Root_inherited> root_inherited = static_cap_cast<Root_inherited>(root);

	/* announce inherited service type */
	announce(Session_type_inherited::service_name(), root_inherited);

	/* recursively announce further inherited session types */
	_announce_base(root_inherited,
	               (Meta::Bool_to_type<Rpc_interface_is_inherited<Session_type_inherited>::VALUE> *)0);
}


#endif /* _INCLUDE__PARENT__PARENT_H_ */
