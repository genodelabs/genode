/*
 * \brief  Child creation framework
 * \author Norman Feske
 * \date   2006-07-22
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CHILD_H_
#define _INCLUDE__BASE__CHILD_H_

#include <base/rpc_server.h>
#include <base/heap.h>
#include <base/process.h>
#include <base/service.h>
#include <base/lock.h>
#include <util/arg_string.h>
#include <parent/parent.h>

namespace Genode {

	/**
	 * Child policy interface
	 *
	 * A child-policy object is an argument to a 'Child'. It is responsible for
	 * taking policy decisions regarding the parent interface. Most importantly,
	 * it defines how session requests are resolved and how session arguments
	 * are passed to servers when creating sessions.
	 */
	struct Child_policy
	{
		virtual ~Child_policy() { }

		/**
		 * Return process name of the child
		 */
		virtual const char *name() const = 0;

		/**
		 * Determine service to provide a session request
		 *
		 * \return  Service to be contacted for the new session, or
		 *          0 if session request could not be resolved
		 */
		virtual Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{ return 0; }

		/**
		 * Apply transformations to session arguments
		 */
		virtual void filter_session_args(const char *service,
		                                 char *args, size_t args_len) { }

		/**
		 * Register a service provided by the child
		 *
		 * \param name   service name
		 * \param root   interface for creating sessions for the service
		 * \param alloc  allocator to be used for child-specific
		 *               meta-data allocations
		 * \return       true if announcement succeeded, or false if
		 *               child is not permitted to announce service
		 */
		virtual bool announce_service(const char            *name,
		                              Root_capability        root,
		                              Allocator             *alloc,
		                              Server                *server)
		{ return false; }

		/**
		 * Unregister services that had been provided by the child
		 */
		virtual void unregister_services() { }

		/**
		 * Exit child
		 */
		virtual void exit(int exit_value)
		{
			PDBG("child exited with exit value %d", exit_value);
		}

		/**
		 * Reference RAM session
		 *
		 * The RAM session returned by this function is used for session-quota
		 * transfers.
		 */
		virtual Ram_session *ref_ram_session() { return env()->ram_session(); }
	};


	/**
	 * Implementation of the parent interface that supports resource trading
	 *
	 * There are three possible cases of how a session can be provided to
	 * a child:
	 *
	 * # The service is implemented locally
	 * # The session was obtained by asking our parent
	 * # The session is provided by one of our children
	 *
	 * These types must be differentiated for the quota management when a child
	 * issues the closing of a session or a transfers quota via our parent
	 * interface.
	 *
	 * If we close a session to a local service, we transfer the session quota
	 * from our own account to the client.
	 *
	 * If we close a parent session, we receive the session quota on our own
	 * account and must transfer this amount to the session-closing child.
	 *
	 * If we close a session provided by a server child, we close the session
	 * at the server, transfer the session quota from the server's ram session
	 * to our account, and subsequently transfer the same amount from our
	 * account to the client.
	 */
	class Child : protected Rpc_object<Parent>
	{
		private:

			class Session;

			/* RAM session that contains the quota of the child */
			Ram_session_capability  _ram;
			Ram_session_client      _ram_session_client;

			/* CPU session that contains the quota of the child */
			Cpu_session_capability  _cpu;

			/* RM session representing the address space of the child */
			Rm_session_capability   _rm;

			/* Services where the RAM, CPU, and RM resources come from */
			Service                &_ram_service;
			Service                &_cpu_service;
			Service                &_rm_service;

			/* heap for child-specific allocations using the child's quota */
			Heap                    _heap;

			Rpc_entrypoint         *_entrypoint;
			Parent_capability       _parent_cap;

			/* child policy */
			Child_policy           *_policy;

			/* sessions opened by the child */
			Lock                    _lock;   /* protect list manipulation */
			Object_pool<Session>    _session_pool;
			List<Session>           _session_list;

			/* server role */
			Server                 _server;

			/**
			 * Session-argument buffer
			 */
			char _args[Parent::Session_args::MAX_SIZE];

			Process _process;

			/**
			 * Attach session information to a child
			 *
			 * \throw Ram_session::Quota_exceeded  the child's heap partition cannot
			 *                                     hold the session meta data
			 */
			void _add_session(const Session &s);

			/**
			 * Close session and revert quota donation associated with it
			 */
			void _remove_session(Session *s);

			/**
			 * Return service interface targetting the parent
			 *
			 * The service returned by this function is used as default
			 * provider for the RAM, CPU, and RM resources of the child. It is
			 * solely used for targeting resource donations during
			 * 'Parent::upgrade_quota()' calls.
			 */
			static Service *_parent_service();

		public:

			/**
			 * Constructor
			 *
			 * \param elf_ds       dataspace containing the binary
			 * \param ram          RAM session with the child's quota
			 * \param cpu          CPU session with the child's quota
			 * \param rm           RM session representing the address space
			 *                     of the child
			 * \param entrypoint   server entrypoint to serve the parent interface
			 * \param policy       child policy
			 * \param ram_service  provider of the 'ram' session
			 * \param cpu_service  provider of the 'cpu' session
			 * \param rm_service   provider of the 'rm' session
			 *
			 * If assigning a separate entry point to each child, the host of
			 * multiple children is able to handle a blocking invocation of
			 * the parent interface of one child while still maintaining the
			 * service to other children, each having an independent entry
			 * point.
			 *
			 * The 'ram_service', 'cpu_service', and 'rm_service' arguments are
			 * needed to direct quota upgrades referring to the resources of
			 * the child environment. By default, we expect that these
			 * resources are provided by the parent.
			 */
			Child(Dataspace_capability    elf_ds,
			      Ram_session_capability  ram,
			      Cpu_session_capability  cpu,
			      Rm_session_capability   rm,
			      Rpc_entrypoint         *entrypoint,
			      Child_policy           *policy,
			      Service                &ram_service = *_parent_service(),
			      Service                &cpu_service = *_parent_service(),
			      Service                &rm_service  = *_parent_service());

			/**
			 * Destructor
			 *
			 * On destruction of a child, we close all sessions of the child to
			 * other services.
			 */
			virtual ~Child();

			/**
			 * Return heap that uses the child's quota
			 */
			Allocator *heap() { return &_heap; }

			Ram_session_capability ram_session_cap() const { return _ram; }
			Cpu_session_capability cpu_session_cap() const { return _cpu; }
			Rm_session_capability   rm_session_cap() const { return _rm;  }
			Parent_capability       parent_cap()     const { return cap(); }

			/**
			 * Discard all sessions to specified service
			 *
			 * When this function is called, we assume the server protection
			 * domain to be dead and all that all server quota was already
			 * transferred back to our own 'env()->ram_session()' account. Note
			 * that the specified server object may not exist anymore. We do
			 * not de-reference the server argument in here!
			 */
			void revoke_server(const Server *server);


			/**********************
			 ** Parent interface **
			 **********************/

			void announce(Service_name const &, Root_capability);
			Session_capability session(Service_name const &, Session_args const &);
			void upgrade(Session_capability, Upgrade_args const &);
			void close(Session_capability);
			void exit(int);
	};
}

#endif /* _INCLUDE__BASE__CHILD_H_ */
