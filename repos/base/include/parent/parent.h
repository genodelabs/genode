/*
 * \brief  Parent interface
 * \author Norman Feske
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PARENT__PARENT_H_
#define _INCLUDE__PARENT__PARENT_H_

#include <base/exception.h>
#include <base/rpc.h>
#include <base/rpc_args.h>
#include <base/thread.h>
#include <base/id_space.h>
#include <session/capability.h>
#include <root/capability.h>

namespace Genode {

	class Session_state;
	class Parent;
}


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

		typedef Rpc_in_buffer<64>  Service_name;
		typedef Rpc_in_buffer<160> Session_args;
		typedef Rpc_in_buffer<160> Upgrade_args;

		struct Client : Interface { typedef Id_space<Client>::Id Id; };
		struct Server : Interface { typedef Id_space<Server>::Id Id; };

		/**
		 * Predefined session IDs corresponding to the environment sessions
		 * created by the parent at the component-construction time
		 */
		struct Env
		{
			static Client::Id pd()      { return { 1 }; }
			static Client::Id cpu()     { return { 2 }; }
			static Client::Id log()     { return { 3 }; }
			static Client::Id binary()  { return { 4 }; }
			static Client::Id linker()  { return { 5 }; }
			static Client::Id last()    { return { 5 }; }

			/**
			 * True if session ID refers to an environment session
			 */
			static bool session_id(Client::Id id) {
				return id.value >= 1 && id.value <= last().value; }
		};

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
		virtual void announce(Service_name const &service_name) = 0;

		/**
		 * Emulation of the original synchronous root interface
		 *
		 * This method transparently spawns a proxy "root" entrypoint that
		 * dispatches asynchronous session-management operations (as issued
		 * by the parent) to the local root interfaces via component-local
		 * RPC calls.
		 *
		 * The method solely exists for API compatibility.
		 */
		static void announce(Service_name const &service_name,
		                     Root_capability service_root);

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
		 * Register signal handler for session notifications
		 */
		virtual void session_sigh(Signal_context_capability) = 0;

		/**
		 * Create session to a service
		 *
		 * \param id               client-side ID of new session
		 * \param service_name     name of the requested interface
		 * \param args             session constructor arguments
		 * \param affinity         preferred CPU affinity for the session
		 *
		 * \throw Service_denied          parent denies session request
		 * \throw Insufficient_cap_quota  donated cap quota does not suffice
		 * \throw Insufficient_ram_quota  donated RAM quota does not suffice
		 * \throw Out_of_caps             session CAP quota exceeds our resources
		 * \throw Out_of_ram              session RAM quota exceeds our resources
		 *
		 * \return session capability if the new session is immediately
		 *         available, or an invalid capability
		 *
		 * If the returned capability is invalid, the request is pending at the
		 * server. The parent delivers a signal to the handler as registered
		 * via 'session_sigh' once the server responded to the request. Now the
		 * session capability can be picked up by calling 'session_cap'.
		 */
		virtual Session_capability session(Client::Id          id,
		                                   Service_name const &service_name,
		                                   Session_args const &args,
		                                   Affinity     const &affinity = Affinity()) = 0;

		/**
		 * Request session capability
		 *
		 * \throw Service_denied
		 * \throw Insufficient_cap_quota
		 * \throw Insufficient_ram_quota
		 *
		 * See 'session' for more documentation.
		 *
		 * In the exception case, the parent implicitly closes the session.
		 */
		virtual Session_capability session_cap(Client::Id id) = 0;

		enum Upgrade_result { UPGRADE_DONE, UPGRADE_PENDING };

		/**
		 * Transfer our quota to the server that provides the specified session
		 *
		 * \param id         ID of recipient session
		 * \param args       description of the amount of quota to transfer
		 *
		 * \throw Out_of_caps
		 * \throw Out_of_ram
		 *
		 * The 'args' argument has the same principle format as the 'args'
		 * argument of the 'session' operation.
		 */
		virtual Upgrade_result upgrade(Client::Id to_session,
		                               Upgrade_args const &args) = 0;

		enum [[nodiscard]] Close_result { CLOSE_DONE, CLOSE_PENDING };

		/**
		 * Close session
		 */
		virtual Close_result close(Client::Id) = 0;

		/*
		 * Interface for providing services
		 */

		enum Session_response { SESSION_OK, SESSION_CLOSED, SERVICE_DENIED,
		                        INSUFFICIENT_RAM_QUOTA, INSUFFICIENT_CAP_QUOTA };

		/**
		 * Set state of a session provided by the child service
		 */
		virtual void session_response(Server::Id, Session_response) = 0;

		/**
		 * Deliver capability for a new session provided by the child service
		 */
		virtual void deliver_session_cap(Server::Id, Session_capability) = 0;

		/*
		 * Dynamic resource balancing
		 */

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
		 * By invoking this method, a component is able to inform its
		 * parent about the need for additional resources. The argument
		 * string contains a resource description in the same format as
		 * used for session-construction arguments. In particular, for
		 * requesting additional RAM quota, the argument looks like
		 * "ram_quota=<amount>" where 'amount' is the amount of additional
		 * resources expected from the parent. If the parent complies with
		 * the request, it submits a resource-available signal to the
		 * handler registered via 'resource_avail_sigh()'. On the reception
		 * of such a signal, the component can re-evaluate its resource quota
		 * and resume execution.
		 */
		virtual void resource_request(Resource_args const &args) = 0;

		/**
		 * Register signal handler for resource yield notifications
		 *
		 * Using the yield signal, the parent is able to inform the component
		 * about its wish to regain resources.
		 */
		virtual void yield_sigh(Signal_context_capability sigh) = 0;

		/**
		 * Obtain information about the amount of resources to free
		 *
		 * The amount of resources returned by this method is the
		 * goal set by the parent. It is not commanded but merely meant
		 * as a friendly beg to cooperate. The component is not obligated
		 * to comply. If the component decides to take action to free
		 * resources, it can inform its parent about the availability
		 * of freed up resources by calling 'yield_response()'.
		 */
		virtual Resource_args yield_request() = 0;

		/**
		 * Notify the parent about a response to a yield request
		 */
		virtual void yield_response() = 0;

		/*
		 * Health monitoring
		 */

		/**
		 * Register heartbeat handler
		 *
		 * The parent may issue heartbeat signals to the child at any time
		 * and expects a call of the 'heartbeat_response' RPC function as
		 * response. When oberving the RPC call, the parent infers that the
		 * child is still able to respond to external events.
		 */
		virtual void heartbeat_sigh(Signal_context_capability sigh) = 0;

		/**
		 * Deliver response to a heartbeat signal
		 */
		virtual void heartbeat_response() = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_exit, void, exit, int);
		GENODE_RPC(Rpc_announce, void, announce,
		           Service_name const &);
		GENODE_RPC(Rpc_session_sigh, void, session_sigh, Signal_context_capability);
		GENODE_RPC_THROW(Rpc_session, Session_capability, session,
		                 GENODE_TYPE_LIST(Service_denied, Out_of_caps,
		                                  Out_of_ram, Insufficient_cap_quota,
		                                  Insufficient_ram_quota),
		                 Client::Id, Service_name const &, Session_args const &,
		                 Affinity const &);
		GENODE_RPC_THROW(Rpc_session_cap, Session_capability, session_cap,
		                 GENODE_TYPE_LIST(Service_denied, Insufficient_cap_quota,
		                                  Insufficient_ram_quota),
		                 Client::Id);
		GENODE_RPC_THROW(Rpc_upgrade, Upgrade_result, upgrade,
		                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
		                 Client::Id, Upgrade_args const &);
		GENODE_RPC(Rpc_close, Close_result, close, Client::Id);
		GENODE_RPC(Rpc_session_response, void, session_response,
		           Server::Id, Session_response);
		GENODE_RPC(Rpc_deliver_session_cap, void, deliver_session_cap,
		           Server::Id, Session_capability);
		GENODE_RPC(Rpc_main_thread, Thread_capability, main_thread_cap);
		GENODE_RPC(Rpc_resource_avail_sigh, void, resource_avail_sigh,
		           Signal_context_capability);
		GENODE_RPC(Rpc_resource_request, void, resource_request,
		           Resource_args const &);
		GENODE_RPC(Rpc_yield_sigh, void, yield_sigh, Signal_context_capability);
		GENODE_RPC(Rpc_yield_request, Resource_args, yield_request);
		GENODE_RPC(Rpc_yield_response, void, yield_response);
		GENODE_RPC(Rpc_heartbeat_sigh, void, heartbeat_sigh, Signal_context_capability);
		GENODE_RPC(Rpc_heartbeat_response, void, heartbeat_response);

		GENODE_RPC_INTERFACE(Rpc_exit, Rpc_announce, Rpc_session_sigh,
		                     Rpc_session, Rpc_session_cap, Rpc_upgrade,
		                     Rpc_close, Rpc_session_response, Rpc_main_thread,
		                     Rpc_deliver_session_cap, Rpc_resource_avail_sigh,
		                     Rpc_resource_request, Rpc_yield_sigh,
		                     Rpc_yield_request, Rpc_yield_response,
		                     Rpc_heartbeat_sigh, Rpc_heartbeat_response);
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
