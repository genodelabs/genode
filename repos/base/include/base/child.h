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
#include <base/local_connection.h>
#include <util/arg_string.h>
#include <ram_session/connection.h>
#include <region_map/client.h>
#include <pd_session/connection.h>
#include <cpu_session/connection.h>
#include <log_session/connection.h>
#include <rom_session/connection.h>
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
	typedef String<64> Name;
	typedef String<64> Binary_name;
	typedef String<64> Linker_name;

	virtual ~Child_policy() { }

	/**
	 * Name of the child used as the child's label prefix
	 */
	virtual Name name() const = 0;

	/**
	 * ROM module name of the binary to start
	 */
	virtual Binary_name binary_name() const { return name(); }

	/**
	 * ROM module name of the dynamic linker
	 */
	virtual Linker_name linker_name() const { return "ld.lib.so"; }

	/**
	 * Determine service to provide a session request
	 *
	 * \return  service to be contacted for the new session
	 *
	 * \throw Parent::Service_denied
	 */
	virtual Service &resolve_session_request(Service::Name       const &,
	                                         Session_state::Args const &) = 0;

	/**
	 * Apply transformations to session arguments
	 */
	virtual void filter_session_args(Service::Name const &,
	                                 char * /*args*/, size_t /*args_len*/) { }

	/**
	 * Register a service provided by the child
	 */
	virtual void announce_service(Service::Name const &) { }

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
	virtual Ram_session           &ref_ram() = 0;
	virtual Ram_session_capability ref_ram_cap() const = 0;

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

	/**
	 * Initialize the child's RAM session
	 *
	 * The function must define the child's reference account and transfer
	 * the child's initial RAM quota.
	 */
	virtual void init(Ram_session &, Capability<Ram_session>) = 0;

	/**
	 * Initialize the child's CPU session
	 *
	 * The function may install an exception signal handler or assign CPU quota
	 * to the child.
	 */
	virtual void init(Cpu_session &, Capability<Cpu_session>) { }

	/**
	 * Initialize the child's PD session
	 *
	 * The function may install a region-map fault handler for the child's
	 * address space ('Pd_session::address_space');.
	 */
	virtual void init(Pd_session &, Capability<Pd_session>) { }

	class Nonexistent_id_space : Exception { };

	/**
	 * ID space for sessions provided by the child
	 *
	 * \throw Nonexistent_id_space
	 */
	virtual Id_space<Parent::Server> &server_id_space() { throw Nonexistent_id_space(); }

	/**
	 * Return region map for the child's address space
	 *
	 * \param pd  the child's PD session capability
	 *
	 * By default, the function returns a 'nullptr'. In this case, the 'Child'
	 * interacts with the address space of the child's PD session via RPC calls
	 * to the 'Pd_session::address_space'.
	 *
	 * By overriding the default, those RPC calls can be omitted, which is
	 * useful if the child's PD session (including the PD's address space) is
	 * virtualized by the parent. If the virtual PD session is served by the
	 * same entrypoint as the child's parent interface, an RPC call to 'pd'
	 * would otherwise produce a deadlock.
	 */
	virtual Region_map *address_space(Pd_session &) { return nullptr; }
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
class Genode::Child : protected Rpc_object<Parent>,
                      Session_state::Ready_callback,
                      Session_state::Closed_callback
{
	private:

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

		/* child policy */
		Child_policy &_policy;

		/* sessions opened by the child */
		Id_space<Client> _id_space;

		typedef Session_state::Args Args;

		template <typename CONNECTION>
		struct Env_connection
		{
			typedef String<64> Label;

			Args const                   _args;
			Service                     &_service;
			Local_connection<CONNECTION> _connection;

			/**
			 * Construct session arguments with the child policy applied
			 */
			Args _construct_args(Child_policy &policy, Label const &label)
			{
				/* copy original arguments into modifiable buffer */
				char buf[Session_state::Args::capacity()];
				buf[0] = 0;

				/* supply label as session argument */
				if (label.valid())
					Arg_string::set_arg_string(buf, sizeof(buf), "label", label.string());

				/* apply policy to argument buffer */
				policy.filter_session_args(CONNECTION::service_name(), buf, sizeof(buf));

				return Session_state::Args(Cstring(buf));
			}

			Env_connection(Child_policy &policy, Id_space<Parent::Client> &id_space,
			               Id_space<Parent::Client>::Id id, Label const &label = Label())
			:
				_args(_construct_args(policy, label)),
				_service(policy.resolve_session_request(CONNECTION::service_name(), _args)),
				_connection(_service, id_space, id, _args,
				            policy.filter_session_affinity(Affinity()))
			{ }

			typedef typename CONNECTION::Session_type SESSION;

			SESSION             &session()  { return _connection.session(); }
			Capability<SESSION> cap() const { return _connection.cap(); }
		};

		Env_connection<Ram_connection> _ram { _policy,
			_id_space, Parent::Env::ram(), _policy.name() };

		Env_connection<Pd_connection> _pd { _policy,
			_id_space, Parent::Env::pd(), _policy.name() };

		Env_connection<Cpu_connection> _cpu { _policy,
			_id_space, Parent::Env::cpu(), _policy.name() };

		Env_connection<Log_connection> _log { _policy,
			_id_space, Parent::Env::log(), _policy.name() };

		Env_connection<Rom_connection> _binary { _policy,
			_id_space, Parent::Env::binary(), _policy.binary_name() };

		Lazy_volatile_object<Env_connection<Rom_connection> > _linker { _policy,
			_id_space, Parent::Env::linker(), _policy.linker_name() };

		/* call 'Child_policy::init' methods for the environment sessions */
		void _init_env_sessions()
		{
			_policy.init(_ram.session(), _ram.cap());
			_policy.init(_cpu.session(), _cpu.cap());
			_policy.init(_pd.session(),  _pd.cap());
		}
		bool const _env_sessions_initialized = ( _init_env_sessions(), true );

		Dataspace_capability _linker_dataspace()
		{
			try {
				_linker.construct(_policy, _id_space,
				                  Parent::Env::linker(), _policy.linker_name());
				return _linker->session().dataspace();
			}
			catch (Parent::Service_denied) { return Rom_dataspace_capability(); }
		}

		/* heap for child-specific allocations using the child's quota */
		Heap _heap;

		/* factory for dynamically created  session-state objects */
		Session_state::Factory _session_factory { _heap };

		Rpc_entrypoint    &_entrypoint;
		Parent_capability  _parent_cap;

		/* signal handlers registered by the child */
		Signal_context_capability _resource_avail_sigh;
		Signal_context_capability _yield_sigh;
		Signal_context_capability _session_sigh;

		/* arguments fetched by the child in response to a yield signal */
		Lock          _yield_request_lock;
		Resource_args _yield_request_args;

		Initial_thread _initial_thread { _cpu.session(), _pd.cap(), "initial" };

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

		void _revert_quota_and_destroy(Session_state &);

		Close_result _close(Session_state &);

		/**
		 * Session_state::Ready_callback
		 */
		void session_ready(Session_state &session) override;

		/**
		 * Session_state::Closed_callback
		 */
		void session_closed(Session_state &) override;

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
		 * \param rm          local address space, usually 'env.rm()'
		 * \param entrypoint  entrypoint used to serve the parent interface of
		 *                    the child
		 * \param policy      policy for the child
		 *
		 * \throw Parent::Service_denied  if the initial sessions for the
		 *                                child's environment could not be
		 *                                opened
		 * \throw Ram_session::Alloc_failed
		 * \throw Process_startup_failed
		 */
		Child(Region_map &rm, Rpc_entrypoint &entrypoint, Child_policy &policy);

		/**
		 * Destructor
		 *
		 * On destruction of a child, we close all sessions of the child to
		 * other services.
		 */
		virtual ~Child();

		/**
		 * RAM quota unconditionally consumed by the child's environment
		 */
		static size_t env_ram_quota()
		{
			return Cpu_connection::RAM_QUOTA + Ram_connection::RAM_QUOTA +
			        Pd_connection::RAM_QUOTA + Log_connection::RAM_QUOTA +
			     2*Rom_connection::RAM_QUOTA;
		}

		/**
		 * Deduce session costs from usable ram quota
		 */
		static size_t effective_ram_quota(size_t const ram_quota)
		{
			if (ram_quota < env_ram_quota())
				return 0;

			return ram_quota - env_ram_quota();
		}

		/**
		 * Return heap that uses the child's quota
		 */
		Allocator &heap() { return _heap; }

		Ram_session_capability ram_session_cap() const { return _ram.cap(); }

		Parent_capability parent_cap() const { return cap(); }

		Ram_session &ram() { return _ram.session(); }
		Cpu_session &cpu() { return _cpu.session(); }
		Pd_session  &pd()  { return _pd .session(); }

		Session_state::Factory &session_factory() { return _session_factory; }

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

		void announce(Service_name const &) override;
		void session_sigh(Signal_context_capability) override;
		Session_capability session(Client::Id, Service_name const &,
		                           Session_args const &, Affinity const &) override;
		Session_capability session_cap(Client::Id) override;
		Upgrade_result upgrade(Client::Id, Upgrade_args const &) override;
		Close_result close(Client::Id) override;
		void exit(int) override;
		void session_response(Server::Id, Session_response) override;
		void deliver_session_cap(Server::Id, Session_capability) override;
		Thread_capability main_thread_cap() const override;
		void resource_avail_sigh(Signal_context_capability) override;
		void resource_request(Resource_args const &) override;
		void yield_sigh(Signal_context_capability) override;
		Resource_args yield_request() override;
		void yield_response() override;
};

#endif /* _INCLUDE__BASE__CHILD_H_ */
