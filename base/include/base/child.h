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

			/**
			 * Representation of an open session
			 */
			class Session : public Object_pool<Session>::Entry,
			                public List<Session>::Element
			{
				private:

					enum { IDENT_LEN = 16 };

					/**
					 * Session capability at the server
					 */
					Session_capability _cap;

					/**
					 * Service interface that was used to create the session
					 */
					Service *_service;

					/**
					 * Server implementing the session
					 *
					 * Even though we can normally determine the server of the
					 * session via '_service->server()', this does not apply
					 * when destructing a server. During destruction, we use
					 * the 'Server' pointer as opaque key for revoking active
					 * sessions of the server. So we keep a copy independent of
					 * the 'Service' object.
					 */
					Server *_server;

					/**
					 * Total of quota associated with this session
					 */
					size_t _donated_ram_quota;

					/**
					 * Name of session, used for debugging
					 */
					char _ident[IDENT_LEN];

				public:

					/**
					 * Constructor
					 *
					 * \param session    session capability
					 * \param service    service that implements the session
					 * \param ram_quota  initial quota donation associated with
					 *                   the session
					 * \param ident      optional session identifier, used for
					 *                   debugging
					 */
					Session(Session_capability session, Service *service,
					        size_t ram_quota, const char *ident = "<noname>")
					:
						Object_pool<Session>::Entry(session), _cap(session),
						_service(service), _server(service->server()),
						_donated_ram_quota(ram_quota) {
						strncpy(_ident, ident, sizeof(_ident)); }

					/**
					 * Default constructor creates invalid session
					 */
					Session() : _service(0), _donated_ram_quota(0) { }

					/**
					 * Extend amount of ram attached to the session
					 */
					void upgrade_ram_quota(size_t ram_quota) {
						_donated_ram_quota += ram_quota; }

					/**
					 * Accessors
					 */
					Session_capability cap()               const { return _cap; }
					size_t             donated_ram_quota() const { return _donated_ram_quota; }
					bool               valid()             const { return _service != 0; }
					Service           *service()           const { return _service; }
					Server            *server()            const { return _server; }
					const char        *ident()             const { return _ident; }
			};


			/**
			 * Guard for transferring quota donation
			 *
			 * This class is used to provide transactional semantics of quota
			 * transfers. Establishing a new session involves several steps, in
			 * particular subsequent quota transfers. If one intermediate step
			 * fails, we need to revert all quota transfers that already took
			 * place. When instantated at a local scope, a 'Transfer' object
			 * guards a quota transfer. If the scope is left without prior an
			 * explicit acknowledgement of the transfer (for example via an
			 * exception), the destructor the 'Transfer' object reverts the
			 * transfer in flight.
			 */
			class Transfer {

				bool                   _ack;
				size_t                 _quantum;
				Ram_session_capability _from;
				Ram_session_capability _to;

				public:

					/**
					 * Constructor
					 *
					 * \param quantim  number of bytes to transfer
					 * \param from     donator RAM session
					 * \param to       receiver RAM session
					 */
					Transfer(size_t quantum,
					         Ram_session_capability from,
					         Ram_session_capability to)
					: _ack(false), _quantum(quantum), _from(from), _to(to)
					{
						if (_from.valid() && _to.valid() &&
						    Ram_session_client(_from).transfer_quota(_to, quantum)) {
							PWRN("not enough quota for a donation of %zd bytes", quantum);
							throw Quota_exceeded();
						}
					}

					/**
					 * Destructor
					 *
					 * The destructor will be called when leaving the scope of
					 * the 'session' function. If the scope is left because of
					 * an error (e.g., an exception), the donation will be
					 * reverted.
					 */
					~Transfer()
					{
						if (!_ack && _from.valid() && _to.valid())
							Ram_session_client(_to).transfer_quota(_from, _quantum);
					}

					/**
					 * Acknowledge quota donation
					 */
					void acknowledge() { _ack = true; }
			};

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
			void _add_session(const Session &s)
			{
				Lock::Guard lock_guard(_lock);

				/*
				 * Store session information in a new child's meta data
				 * structure.  The allocation from 'heap()' may throw a
				 * 'Ram_session::Quota_exceeded' exception.
				 */
				Session *session = 0;
				try {
					session = new (heap())
					          Session(s.cap(), s.service(),
					                  s.donated_ram_quota(), s.ident()); }
				catch (Allocator::Out_of_memory) {
					throw Parent::Quota_exceeded(); }

				/* these functions may also throw 'Ram_session::Quota_exceeded' */
				_session_pool.insert(session);
				_session_list.insert(session);
			}

			/**
			 * Close session and revert quota donation associated with it
			 */
			void _remove_session(Session *s)
			{
				/* forget about this session */
				_session_pool.remove(s);
				_session_list.remove(s);

				/* return session quota to the ram session of the child */
				if (_policy->ref_ram_session()->transfer_quota(_ram, s->donated_ram_quota()))
					PERR("We ran out of our own quota");

				destroy(heap(), s);
			}

			/**
			 * Return service interface targetting the parent
			 *
			 * The service returned by this function is used as default
			 * provider for the RAM, CPU, and RM resources of the child. It is
			 * solely used for targeting resource donations during
			 * 'Parent::upgrade_quota()' calls.
			 */
			static Service *_parent_service()
			{
				static Parent_service parent_service("");
				return &parent_service;
			}

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
			      Service                &rm_service  = *_parent_service())
			:
				_ram(ram), _ram_session_client(ram), _cpu(cpu), _rm(rm),
				_ram_service(ram_service), _cpu_service(cpu_service),
				_rm_service(rm_service),
				_heap(&_ram_session_client, env()->rm_session()),
				_entrypoint(entrypoint),
				_parent_cap(_entrypoint->manage(this)),
				_policy(policy),
				_server(ram),
				_process(elf_ds, ram, cpu, rm, _parent_cap, policy->name(), 0)
			{ }

			/**
			 * Destructor
			 *
			 * On destruction of a child, we close all sessions of the child to
			 * other services.
			 */
			virtual ~Child()
			{
				_entrypoint->dissolve(this);
				_policy->unregister_services();

				for (Session *s; (s = _session_pool.first()); )
					close(s->cap());
			}

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
			void revoke_server(const Server *server)
			{
				Lock::Guard lock_guard(_lock);

				while (1) {
					/* search session belonging to the specified server */
					Session *s = _session_list.first();
					for ( ; s && (s->server() != server); s = s->next());

					/* if no matching session exists, we are done */
					if (!s) return;

					_remove_session(s);
				}
			}


			/**********************
			 ** Parent interface **
			 **********************/

			void announce(Service_name const &name, Root_capability root)
			{
				if (!name.is_valid_string()) return;

				_policy->announce_service(name.string(), root, heap(), &_server);
			}

			Session_capability session(Service_name const &name, Session_args const &args)
			{
				if (!name.is_valid_string() || !args.is_valid_string()) throw Unavailable();

				/* return sessions that we created for the child */
				if (!strcmp("Env::ram_session", name.string())) return _ram;
				if (!strcmp("Env::cpu_session", name.string())) return _cpu;
				if (!strcmp("Env::rm_session",  name.string())) return _rm;
				if (!strcmp("Env::pd_session",  name.string())) return _process.pd_session_cap();

				/* filter session arguments according to the child policy */
				strncpy(_args, args.string(), sizeof(_args));
				_policy->filter_session_args(name.string(), _args, sizeof(_args));

				/* transfer the quota donation from the child's account to ourself */
				size_t ram_quota = Arg_string::find_arg(_args, "ram_quota").long_value(0);

				Transfer donation_from_child(ram_quota, _ram, env()->ram_session_cap());

				Service *service = _policy->resolve_session_request(name.string(), _args);

				/* raise an error if no matching service provider could be found */
				if (!service)
					throw Service_denied();

				/* transfer session quota from ourself to the service provider */
				Transfer donation_to_service(ram_quota, env()->ram_session_cap(),
				                             service->ram_session_cap());

				/* create session */
				Session_capability cap;
				try { cap = service->session(_args); }
				catch (Service::Invalid_args)   { throw Service_denied(); }
				catch (Service::Unavailable)    { throw Service_denied(); }
				catch (Service::Quota_exceeded) { throw Quota_exceeded(); }

				/* register session */
				try { _add_session(Session(cap, service, ram_quota, name.string())); }
				catch (Ram_session::Quota_exceeded) { throw Quota_exceeded(); }

				/* finish transaction */
				donation_from_child.acknowledge();
				donation_to_service.acknowledge();

				return cap;
			}

			void upgrade(Session_capability to_session, Upgrade_args const &args)
			{
				Service *targeted_service = 0;

				/* check of upgrade refers to an Env:: resource */
				if (to_session.local_name() == _ram.local_name())
					targeted_service = &_ram_service;
				if (to_session.local_name() == _cpu.local_name())
					targeted_service = &_cpu_service;
				if (to_session.local_name() == _rm.local_name())
					targeted_service = &_rm_service;

				/* check if upgrade refers to server */
				Session * const session = _session_pool.obj_by_cap(to_session);
				if (session)
					targeted_service = session->service();

				if (!targeted_service) {
					PWRN("could not lookup service for session upgrade");
					return;
				}

				if (!args.is_valid_string()) {
					PWRN("no valid session-upgrade arguments");
					return;
				}

				size_t const ram_quota =
					Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);

				/* transfer quota from client to ourself */
				Transfer donation_from_child(ram_quota, _ram,
				                             env()->ram_session_cap());

				/* transfer session quota from ourself to the service provider */
				Transfer donation_to_service(ram_quota, env()->ram_session_cap(),
				                             targeted_service->ram_session_cap());

				try { targeted_service->upgrade(to_session, args.string()); }
				catch (Service::Quota_exceeded) { throw Quota_exceeded(); }

				/* remember new amount attached to the session */
				if (session)
					session->upgrade_ram_quota(ram_quota);

				/* finish transaction */
				donation_from_child.acknowledge();
				donation_to_service.acknowledge();
			}

			void close(Session_capability session_cap)
			{
				/* refuse to close the child's initial sessions */
				if (session_cap.local_name() == _ram.local_name()
				 || session_cap.local_name() == _cpu.local_name()
				 || session_cap.local_name() == _rm.local_name()
				 || session_cap.local_name() == _process.pd_session_cap().local_name())
					return;

				Session *s = _session_pool.obj_by_cap(session_cap);

				if (!s) {
					PWRN("no session structure found");
					return;
				}

				/*
				 * There is a chance that the server is not responding to
				 * the 'close' call, making us block infinitely. However,
				 * by using core's cancel-blocking mechanism, we can cancel
				 * the 'close' call by another (watchdog) thread that
				 * invokes 'cancel_blocking' at our thread after a timeout.
				 * The unblocking is reflected at the API level as an
				 * 'Blocking_canceled' exception. We catch this exception
				 * to proceed with normal operation after being unblocked.
				 */
				try { s->service()->close(s->cap()); }
				catch (Blocking_canceled) {
					PDBG("Got Blocking_canceled exception during %s->close call\n",
					     s->ident()); }

				/*
				 * If the session was provided by a child of us,
				 * 'server()->ram_session_cap()' returns the RAM session of the
				 * corresponding child. Since the session to the server is
				 * closed now, we expect that the server released all donated
				 * resources and we can decrease the servers' quota.
				 *
				 * If this goes wrong, the server is misbehaving.
				 */
				if (s->service()->ram_session_cap().valid()) {
					Ram_session_client server_ram(s->service()->ram_session_cap());
					if (server_ram.transfer_quota(env()->ram_session_cap(),
					                              s->donated_ram_quota())) {
						PERR("Misbehaving server '%s'!", s->service()->name());
					}
				}

				Lock::Guard lock_guard(_lock);
				_remove_session(s);
			}

			void exit(int exit_value)
			{
				/*
				 * This function receives the hint from the child that now, its
				 * a good time to kill it. An inherited child class could use
				 * this hint to schedule the destruction of the child object.
				 *
				 * Note that the child object must not be destructed from by
				 * this function because it is executed by the thread contained
				 * in the child object.
				 */
				return _policy->exit(exit_value);
			}
	};
}

#endif /* _INCLUDE__BASE__CHILD_H_ */
