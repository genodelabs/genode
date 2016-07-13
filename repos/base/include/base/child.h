/*
 * \brief  Child creation framework
 * \author Norman Feske
 * \date   2006-07-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CHILD_H_
#define _INCLUDE__BASE__CHILD_H_

#include <base/rpc_server.h>
#include <base/heap.h>
#include <base/service.h>
#include <base/lock.h>
#include <util/arg_string.h>
#include <ram_session/capability.h>
#include <region_map/client.h>
#include <pd_session/client.h>
#include <cpu_session/client.h>
#include <parent/capability.h>

namespace Genode {
	struct Child_policy;
	struct Child;
}


/**
 * Child policy interface
 *
 * A child-policy object is an argument to a 'Child'. It is responsible for
 * taking policy decisions regarding the parent interface. Most importantly,
 * it defines how session requests are resolved and how session arguments
 * are passed to servers when creating sessions.
 */
struct Genode::Child_policy
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
	virtual Service *resolve_session_request(const char * /*service_name*/,
	                                         const char * /*args*/)
	{ return 0; }

	/**
	 * Apply transformations to session arguments
	 */
	virtual void filter_session_args(const char * /*service*/,
	                                 char * /*args*/, size_t /*args_len*/) { }

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
	virtual bool announce_service(const char            * /*name*/,
	                              Root_capability         /*root*/,
	                              Allocator             * /*alloc*/,
	                              Server                * /*server*/)
	{ return false; }

	/**
	 * Apply session affinity policy
	 *
	 * \param affinity  affinity passed along with a session request
	 * \return          affinity subordinated to the child policy
	 */
	virtual Affinity filter_session_affinity(Affinity const &affinity)
	{
		return affinity;
	}

	/**
	 * Unregister services that had been provided by the child
	 */
	virtual void unregister_services() { }

	/**
	 * Exit child
	 */
	virtual void exit(int exit_value)
	{
		log("child \"", name(), "\" exited with exit value ", exit_value);
	}

	/**
	 * Reference RAM session
	 *
	 * The RAM session returned by this method is used for session-quota
	 * transfers.
	 */
	virtual Ram_session *ref_ram_session() { return env()->ram_session(); }
	virtual Ram_session_capability ref_ram_cap() const { return env()->ram_session_cap(); }

	/**
	 * Respond to the release of resources by the child
	 *
	 * This method is called when the child confirms the release of
	 * resources in response to a yield request.
	 */
	virtual void yield_response() { }

	/**
	 * Take action on additional resource needs by the child
	 */
	virtual void resource_request(Parent::Resource_args const &) { }
};


/**
 * Implementation of the parent interface that supports resource trading
 *
 * There are three possible cases of how a session can be provided to
 * a child: The service is implemented locally, the session was obtained by
 * asking our parent, or the session is provided by one of our children.
 *
 * These types must be differentiated for the quota management when a child
 * issues the closing of a session or transfers quota via our parent
 * interface.
 *
 * If we close a session to a local service, we transfer the session quota
 * from our own account to the client.
 *
 * If we close a parent session, we receive the session quota on our own
 * account and must transfer this amount to the session-closing child.
 *
 * If we close a session provided by a server child, we close the session
 * at the server, transfer the session quota from the server's RAM session
 * to our account, and subsequently transfer the same amount from our
 * account to the client.
 */
class Genode::Child : protected Rpc_object<Parent>
{
	public:

		struct Initial_thread_base
		{
			/**
			 * Start execution at specified instruction pointer
			 */
			virtual void start(addr_t ip) = 0;

			/**
			 * Return capability of the initial thread
			 */
			virtual Capability<Cpu_thread> cap() = 0;
		};

		struct Initial_thread : Initial_thread_base
		{
			private:

				Cpu_session      &_cpu;
				Thread_capability _cap;

			public:

				typedef Cpu_session::Name Name;

				/**
				 * Constructor
				 *
				 * \throw Cpu_session::Thread_creation_failed
				 * \throw Cpu_session::Out_of_metadata
				 */
				Initial_thread(Cpu_session &, Pd_session_capability, Name const &);
				~Initial_thread();

				void start(addr_t) override;

				Capability<Cpu_thread> cap() { return _cap; }
		};

	private:

		class Session;

		/* PD session representing the protection domain of the child */
		Pd_session_capability   _pd;

		/* RAM session that contains the quota of the child */
		Ram_session_capability  _ram;

		/* CPU session that contains the quota of the child */
		Cpu_session_capability  _cpu;

		/* services where the PD, RAM, and CPU resources come from */
		Service                &_pd_service;
		Service                &_ram_service;
		Service                &_cpu_service;

		/* heap for child-specific allocations using the child's quota */
		Heap                    _heap;

		Rpc_entrypoint         &_entrypoint;
		Parent_capability       _parent_cap;

		/* child policy */
		Child_policy           &_policy;

		/* sessions opened by the child */
		Lock                    _lock;   /* protect list manipulation */
		Object_pool<Session>    _session_pool;
		List<Session>           _session_list;

		/* server role */
		Server                 _server;

		/* session-argument buffer */
		char _args[Parent::Session_args::MAX_SIZE];

		/* signal handlers registered by the child */
		Signal_context_capability _resource_avail_sigh;
		Signal_context_capability _yield_sigh;

		/* arguments fetched by the child in response to a yield signal */
		Lock          _yield_request_lock;
		Resource_args _yield_request_args;

		struct Process
		{
			class Missing_dynamic_linker : Exception { };
			class Invalid_executable     : Exception { };

			Initial_thread_base &initial_thread;

			struct Loaded_executable
			{
				/**
				 * Initial instruction pointer of the new process, as defined
				 * in the header of the executable.
				 */
				addr_t entry;

				/**
				 * Constructor parses the executable and sets up segment
				 * dataspaces
				 *
				 * \param local_rm  local address space, needed to make the
				 *                  segment dataspaces temporarily visible in
				 *                  the local address space to initialize their
				 *                  content with the data from the 'elf_ds'
				 *
				 * \throw Region_map::Attach_failed
				 * \throw Invalid_executable
				 * \throw Missing_dynamic_linker
				 * \throw Ram_session::Alloc_failed
				 */
				Loaded_executable(Dataspace_capability elf_ds,
				                  Dataspace_capability ldso_ds,
				                  Ram_session &ram,
				                  Region_map &local_rm,
				                  Region_map &remote_rm,
				                  Parent_capability parent_cap);
			} loaded_executable;

			/**
			 * Constructor
			 *
			 * \param ram     RAM session used to allocate the BSS and
			 *                DATA segments for the new process
			 * \param parent  parent of the new protection domain
			 * \param name    name of protection domain
			 *
			 * \throw Missing_dynamic_linker
			 * \throw Invalid_executable
			 * \throw Region_map::Attach_failed
			 * \throw Ram_session::Alloc_failed
			 *
			 * The other arguments correspond to those of 'Child::Child'.
			 *
			 * On construction of a protection domain, the initial thread is
			 * started immediately.
			 *
			 * The argument 'elf_ds' may be invalid to create an empty process.
			 * In this case, all process initialization steps except for the
			 * creation of the initial thread must be done manually, i.e., as
			 * done for implementing fork.
			 */
			Process(Dataspace_capability  elf_ds,
			        Dataspace_capability  ldso_ds,
			        Pd_session_capability pd_cap,
			        Pd_session           &pd,
			        Ram_session          &ram,
			        Initial_thread_base  &initial_thread,
			        Region_map           &local_rm,
			        Region_map           &remote_rm,
			        Parent_capability     parent);

			~Process();
		};

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

		void _close(Session *s);

		/**
		 * Return service interface targetting the parent
		 *
		 * The service returned by this method is used as default
		 * provider for the RAM, CPU, and RM resources of the child. It is
		 * solely used for targeting resource donations during
		 * 'Parent::upgrade_quota()' calls.
		 */
		static Service &_parent_service();

	public:

		/**
		 * Exception type
		 *
		 * The startup of the physical process of the child may fail if the
		 * ELF binary is invalid, if the ELF binary is dynamically linked
		 * but no dynamic linker is provided, if the creation of the initial
		 * thread failed, or if the RAM session of the child is exhausted.
		 * Each of those conditions will result in a diagnostic log message.
		 * But for the error handling, we only distinguish the RAM exhaustion
		 * from the other conditions and subsume the latter as
		 * 'Process_startup_failed'.
		 */
		class Process_startup_failed : public Exception { };

		/**
		 * Constructor
		 *
		 * \param elf_ds          dataspace that contains the ELF binary
		 * \param ldso_ds         dataspace that contains the dynamic linker,
		 *                        started if 'elf_ds' is a dynamically linked
		 *                        executable
		 * \param pd_cap          capability of the new protection domain,
		 *                        used as argument for creating the initial
		 *                        thread, and handed out to the child as its
		 *                        environment
		 * \param pd              PD session used for assigning the parent
		 *                        capability of the new process
		 * \param ram_cap         RAM session capability handed out to the
		 *                        child as its environment
		 * \param ram             RAM session used to allocate the BSS and
		 *                        DATA segments and as backing store for the
		 *                        local heap partition to keep child-specific
		 *                        meta data
		 * \param cpu_cap         CPU session capability handed out to the
		 *                        child as its environment
		 * \param initial_thread  initial thread of the new protection domain
		 * \param local_rm        local address space
		 * \param remote_rm       address space of new protection domain
		 * \param pd_service      provider of the 'pd' session
		 * \param ram_service     provider of the 'ram' session
		 * \param cpu_service     provider of the 'cpu' session
		 *
		 * \throw Ram_session::Alloc_failed
		 * \throw Process_startup_failed
		 *
		 * Usually, the pairs of 'pd' and 'pd_cap', 'initial_thread' and
		 * 'cpu_cap', 'ram' and 'ram_cap' belong to each other. References to
		 * the session interfaces are passed as separate arguments in addition
		 * to the capabilities to allow the creator of a child to operate on
		 * locally implemented interfaces during the child initialization.
		 *
		 * The 'ram_service', 'cpu_service', and 'pd_service' arguments are
		 * needed to direct quota upgrades referring to the resources of
		 * the child environment. By default, we expect that these
		 * resources are provided by the parent.
		 */
		Child(Dataspace_capability    elf_ds,
		      Dataspace_capability    ldso_ds,
		      Pd_session_capability   pd_cap,
		      Pd_session             &pd,
		      Ram_session_capability  ram_cap,
		      Ram_session            &ram,
		      Cpu_session_capability  cpu_cap,
		      Initial_thread_base    &initial_thread,
		      Region_map             &local_rm,
		      Region_map             &remote_rm,
		      Rpc_entrypoint         &entrypoint,
		      Child_policy           &policy,
		      Service                &pd_service  = _parent_service(),
		      Service                &ram_service = _parent_service(),
		      Service                &cpu_service = _parent_service());

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

		Pd_session_capability  pd_session_cap()  const { return _pd; }
		Ram_session_capability ram_session_cap() const { return _ram; }
		Cpu_session_capability cpu_session_cap() const { return _cpu; }
		Parent_capability      parent_cap()      const { return cap(); }

		/**
		 * Discard all sessions to specified service
		 *
		 * When this method is called, we assume the server protection
		 * domain to be dead and all that all server quota was already
		 * transferred back to our own 'env()->ram_session()' account. Note
		 * that the specified server object may not exist anymore. We do
		 * not de-reference the server argument in here!
		 */
		void revoke_server(const Server *server);

		/**
		 * Instruct the child to yield resources
		 *
		 * By calling this method, the child will be notified about the
		 * need to release the specified amount of resources. For more
		 * details about the protocol between a child and its parent,
		 * refer to the description given in 'parent/parent.h'.
		 */
		void yield(Resource_args const &args);

		/**
		 * Notify the child about newly available resources
		 */
		void notify_resource_avail() const;


		/**********************
		 ** Parent interface **
		 **********************/

		void announce(Service_name const &, Root_capability) override;
		Session_capability session(Service_name const &, Session_args const &,
		                           Affinity const &) override;
		void upgrade(Session_capability, Upgrade_args const &) override;
		void close(Session_capability) override;
		void exit(int) override;
		Thread_capability main_thread_cap() const override;
		void resource_avail_sigh(Signal_context_capability) override;
		void resource_request(Resource_args const &) override;
		void yield_sigh(Signal_context_capability) override;
		Resource_args yield_request() override;
		void yield_response() override;
};

#endif /* _INCLUDE__BASE__CHILD_H_ */
