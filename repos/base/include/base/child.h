/*
 * \brief  Child creation framework
 * \author Norman Feske
 * \date   2006-07-22
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__CHILD_H_
#define _INCLUDE__BASE__CHILD_H_

#include <base/rpc_server.h>
#include <base/heap.h>
#include <base/service.h>
#include <base/mutex.h>
#include <base/local_connection.h>
#include <base/quota_guard.h>
#include <util/arg_string.h>
#include <region_map/client.h>
#include <pd_session/connection.h>
#include <cpu_session/connection.h>
#include <log_session/connection.h>
#include <rom_session/connection.h>
#include <cpu_thread/client.h>
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
	 * Routing destination of a session request
	 */
	struct Route
	{
		Service &service;
		Session::Label const label;
		Session::Diag  const diag;
	};

	/**
	 * Determine service and server-side label for a given session request
	 *
	 * \return  routing and policy-selection information for the session
	 *
	 * \throw Service_denied
	 */
	virtual Route resolve_session_request(Service::Name const &,
	                                      Session_label const &,
	                                      Session::Diag) = 0;

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
	 * Reference PD session
	 *
	 * The PD session returned by this method is used for session cap-quota
	 * and RAM-quota transfers.
	 */
	virtual Pd_session           &ref_pd() = 0;
	virtual Pd_session_capability ref_pd_cap() const = 0;

	/**
	 * RAM allocator used as backing store for '_session_md_alloc'
	 */
	virtual Ram_allocator &session_md_ram() { return ref_pd(); }

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
	 * Initialize the child's CPU session
	 *
	 * The function may install an exception signal handler or assign CPU quota
	 * to the child.
	 */
	virtual void init(Cpu_session &, Capability<Cpu_session>) { }

	/**
	 * Initialize the child's PD session
	 *
	 * The function must define the child's reference account and transfer
	 * the child's initial RAM and capability quotas. It may also install a
	 * region-map fault handler for the child's address space
	 * ('Pd_session::address_space');.
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
	 * Notification hook invoked each time a session state is modified
	 */
	virtual void session_state_changed() { }

	/**
	 * Granularity of allocating the backing store for session meta data
	 *
	 * Session meta data is allocated from 'ref_pd'. The first batch of
	 * session-state objects is allocated at child-construction time.
	 */
	virtual size_t session_alloc_batch_size() const { return 16; }

	/**
	 * Return true to create the environment sessions at child construction
	 *
	 * By returning 'false', it is possible to create 'Child' objects without
	 * routing of their environment sessions at construction time. Once the
	 * routing information is available, the child's environment sessions
	 * must be manually initiated by calling 'Child::initiate_env_sessions()'.
	 */
	virtual bool initiate_env_sessions() const { return true; }

	struct With_address_space_fn : Interface
	{
		virtual void call(Region_map &) const = 0;
	};

	virtual void _with_address_space(Pd_session &pd, With_address_space_fn const &fn)
	{
		Region_map_client region_map(pd.address_space());
		fn.call(region_map);
	}

	/**
	 * Call functor 'fn' with the child's address-space region map as argument
	 *
	 * In the common case where the child's PD is provided by core, the address
	 * space is accessed via the 'Region_map' RPC interface. However, in cases
	 * where the child's PD session interface is locally implemented - as is
	 * the case for a debug monitor - the address space must be accessed by
	 * component-local method calls instead.
	 */
	template <typename FN>
	void with_address_space(Pd_session &pd, FN const &fn)
	{
		struct Impl : With_address_space_fn
		{
			FN const &_fn;
			Impl(FN const &fn) : _fn(fn) { };
			void call(Region_map &rm) const override { _fn(rm); }
		};

		_with_address_space(pd, Impl(fn));
	}

	/**
	 * Start initial thread of the child at instruction pointer 'ip'
	 */
	virtual void start_initial_thread(Capability<Cpu_thread> thread, addr_t ip)
	{
		Cpu_thread_client(thread).start(ip, 0);
	}

	/**
	 * Return true if ELF loading should be inhibited
	 */
	virtual bool forked() const { return false; }
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

		struct Initial_thread_base : Interface
		{
			struct Start : Interface
			{
				virtual void start_initial_thread(Capability<Cpu_thread>, addr_t ip) = 0;
			};

			/**
			 * Start execution at specified instruction pointer
			 *
			 * The 'Child_policy' allows for the overriding of the default
			 * RPC-based implementation of 'start_initial_thread'.
			 */
			virtual void start(addr_t ip, Start &) = 0;

			/**
			 * Return capability of the initial thread
			 */
			virtual Capability<Cpu_thread> cap() const = 0;
		};

		struct Initial_thread : Initial_thread_base
		{
			private:

				Cpu_session      &_cpu;
				Thread_capability _cap;

			public:

				using Initial_thread_base::Start;

				typedef Cpu_session::Name Name;

				/**
				 * Constructor
				 *
				 * \throw Cpu_session::Thread_creation_failed
				 * \throw Out_of_ram
				 * \throw Out_of_caps
				 */
				Initial_thread(Cpu_session &, Pd_session_capability, Name const &);
				~Initial_thread();

				void start(addr_t, Start &) override;

				Capability<Cpu_thread> cap() const override { return _cap; }
		};

		/* child policy */
		Child_policy &_policy;

		/* print error message with the child's name prepended */
		template <typename... ARGS>
		void _error(ARGS &&... args) { error(_policy.name(), ": ", args...); }

		Region_map &_local_rm;

		Capability_guard _parent_cap_guard;

		/* signal handlers registered by the child */
		Signal_context_capability _resource_avail_sigh { };
		Signal_context_capability _yield_sigh          { };
		Signal_context_capability _session_sigh        { };
		Signal_context_capability _heartbeat_sigh      { };

		/* arguments fetched by the child in response to a yield signal */
		Mutex         _yield_request_mutex { };
		Resource_args _yield_request_args  { };

		/* number of unanswered heartbeat signals */
		unsigned _outstanding_heartbeats = 0;

		/* sessions opened by the child */
		Id_space<Client> _id_space { };

		/* allocator used for dynamically created session state objects */
		Sliced_heap _session_md_alloc { _policy.session_md_ram(), _local_rm };

		Session_state::Factory::Batch_size const
			_session_batch_size { _policy.session_alloc_batch_size() };

		/* factory for dynamically created  session-state objects */
		Session_state::Factory _session_factory { _session_md_alloc,
		                                          _session_batch_size };

		typedef Session_state::Args Args;

		/*
		 * Members that are initialized not before the child's environment is
		 * complete.
		 */

		void _try_construct_env_dependent_members();

		Constructible<Initial_thread> _initial_thread { };

		struct Initial_thread_start : Initial_thread::Start
		{
			Child_policy &_policy;

			void start_initial_thread(Capability<Cpu_thread> cap, addr_t ip) override
			{
				_policy.start_initial_thread(cap, ip);
			}

			Initial_thread_start(Child_policy &policy) : _policy(policy) { }

		} _initial_thread_start { _policy };

		struct Process
		{
			class Missing_dynamic_linker : Exception { };
			class Invalid_executable     : Exception { };

			enum Type { TYPE_LOADED, TYPE_FORKED };

			struct Loaded_executable
			{
				/**
				 * Initial instruction pointer of the new process, as defined
				 * in the header of the executable.
				 */
				addr_t entry { 0 };

				/**
				 * Constructor parses the executable and sets up segment
				 * dataspaces
				 *
				 * \param local_rm  local address space, needed to make the
				 *                  segment dataspaces temporarily visible in
				 *                  the local address space to initialize their
				 *                  content with the data from the 'elf_ds'
				 *
				 * \throw Region_map::Region_conflict
				 * \throw Region_map::Invalid_dataspace
				 * \throw Invalid_executable
				 * \throw Missing_dynamic_linker
				 * \throw Out_of_ram
				 * \throw Out_of_caps
				 */
				Loaded_executable(Type type,
				                  Dataspace_capability ldso_ds,
				                  Ram_allocator &ram,
				                  Region_map &local_rm,
				                  Region_map &remote_rm,
				                  Parent_capability parent_cap);
			} loaded_executable;

			/**
			 * Constructor
			 *
			 * \throw Missing_dynamic_linker
			 * \throw Invalid_executable
			 * \throw Region_map::Region_conflict
			 * \throw Region_map::Invalid_dataspace
			 * \throw Out_of_ram
			 * \throw Out_of_caps
			 *
			 * On construction of a protection domain, the initial thread is
			 * started immediately.
			 *
			 * The 'type' 'TYPE_FORKED' creates an empty process. In this case,
			 * all process initialization steps except for the creation of the
			 * initial thread must be done manually, i.e., as done for
			 * implementing fork.
			 */
			Process(Type                   type,
			        Dataspace_capability   ldso_ds,
			        Pd_session            &pd,
			        Initial_thread_base   &,
			        Initial_thread::Start &,
			        Region_map            &local_rm,
			        Region_map            &remote_rm,
			        Parent_capability      parent);

			~Process();
		};

		Constructible<Process> _process { };

		/*
		 * The child's environment sessions
		 */

		template <typename CONNECTION>
		struct Env_connection
		{
			Child &_child;

			Id_space<Parent::Client>::Id const _client_id;

			typedef String<64> Label;

			Args const _args;

			/*
			 * The 'Env_service' monitors session responses in order to attempt
			 * to 'Child::_try_construct_env_dependent_members()' on the
			 * arrival of environment sessions.
			 */
			struct Env_service : Service, Session_state::Ready_callback
			{
				Child   &_child;
				Service &_service;

				Env_service(Child &child, Service &service)
				:
					Genode::Service(CONNECTION::service_name()),
					_child(child), _service(service)
				{ }

				/*
				 * The 'Local_connection' may call 'initial_request' multiple
				 * times. We use 'initial_request' as a hook to transfer the
				 * session quota to an async service but we want to transfer
				 * the session quota only once. The '_first_request' allows
				 * us to distinguish the first from subsequent calls.
				 */
				bool _first_request = true;

				void initiate_request(Session_state &session) override
				{
					session.ready_callback = this;
					session.async_client_notify = true;

					_service.initiate_request(session);

					/*
					 * If the env session is provided by an async service,
					 * transfer the session resources.
					 */
					bool const async_service =
						session.phase == Session_state::CREATE_REQUESTED;

					if (_first_request && async_service
					 && _service.name() != Pd_session::service_name()) {

						Ram_quota const ram_quota { CONNECTION::RAM_QUOTA };
						Cap_quota const cap_quota { CONNECTION::CAP_QUOTA };

						if (cap(ram_quota).valid())
							_child._policy.ref_pd().transfer_quota(cap(ram_quota), ram_quota);

						if (cap(cap_quota).valid())
							_child._policy.ref_pd().transfer_quota(cap(cap_quota), cap_quota);

						_first_request = false;
					}

					if (session.phase == Session_state::SERVICE_DENIED)
						error(_child._policy.name(), ": environment ",
						      CONNECTION::service_name(), " session denied "
						      "(", session.args(), ")");

					/*
					 * If the env session is provided by an async service,
					 * we have to wake up the server when closing the env
					 * session.
					 */
					if (session.phase == Session_state::CLOSE_REQUESTED)
						_service.wakeup();
				}

				/**
				 * Session_state::Ready_callback
				 */
				void session_ready(Session_state &) override
				{
					_child._try_construct_env_dependent_members();
				}

				/**
				 * Service (Ram_transfer::Account) interface
				 */
				void transfer(Pd_session_capability to, Ram_quota amount) override
				{
					Ram_transfer::Account &from = _service;
					from.transfer(to, amount);
				}

				/**
				 * Service (Ram_transfer::Account) interface
				 */
				Pd_session_capability cap(Ram_quota) const override
				{
					Ram_transfer::Account &to = _service;
					return to.cap(Ram_quota());
				}

				/**
				 * Service (Cap_transfer::Account) interface
				 */
				void transfer(Pd_session_capability to, Cap_quota amount) override
				{
					Cap_transfer::Account &from = _service;
					from.transfer(to, amount);
				}

				/**
				 * Service (Cap_transfer::Account) interface
				 */
				Pd_session_capability cap(Cap_quota) const override
				{
					Cap_transfer::Account &to = _service;
					return to.cap(Cap_quota());
				}

				void wakeup() override { _service.wakeup(); }

				bool operator == (Service const &other) const override
				{
					return _service == other;
				}
			};

			Constructible<Env_service> _env_service { };

			Constructible<Local_connection<CONNECTION> > _connection { };

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

			static char const *_service_name() { return CONNECTION::service_name(); }

			Env_connection(Child &child, Id_space<Parent::Client>::Id id,
			               Label const &label = Label())
			:
				_child(child), _client_id(id),
				_args(_construct_args(child._policy, label))
			{ }

			/**
			 * Initiate routing and creation of the environment session
			 */
			void initiate()
			{
				/* don't attempt to initiate env session twice */
				if (_connection.constructed())
					return;

				try {
					Child_policy::Route const route =
						_child._policy.resolve_session_request(_service_name(),
						                                       label_from_args(_args.string()),
						                                       session_diag_from_args(_args.string()));
					_env_service.construct(_child, route.service);

					Affinity const affinity =
						_child._policy.filter_session_affinity(Affinity::unrestricted());

					_connection.construct(*_env_service, _child._id_space, _client_id,
					                      _args, affinity, route.label, route.diag);
				}
				catch (Service_denied) {
					error(_child._policy.name(), ": ", _service_name(), " "
					      "environment session denied (", _args.string(), ")"); }
			}

			typedef typename CONNECTION::Session_type SESSION;

			SESSION       &session()       { return _connection->session(); }
			SESSION const &session() const { return _connection->session(); }

			Capability<SESSION> cap() const {
				return _connection.constructed() ? _connection->cap()
				                                 : Capability<SESSION>(); }

			bool closed() const { return !_connection.constructed() || _connection->closed(); }

			void close() { if (_connection.constructed()) _connection->close(); }
		};

		Env_connection<Pd_connection>  _pd     { *this, Env::pd(),     _policy.name() };
		Env_connection<Cpu_connection> _cpu    { *this, Env::cpu(),    _policy.name() };
		Env_connection<Log_connection> _log    { *this, Env::log(),    _policy.name() };
		Env_connection<Rom_connection> _binary { *this, Env::binary(), _policy.binary_name() };

		Constructible<Env_connection<Rom_connection> > _linker { };

		Dataspace_capability _linker_dataspace()
		{
			return _linker.constructed() ? _linker->session().dataspace()
			                             : Rom_dataspace_capability();
		}

		void _revert_quota_and_destroy(Session_state &);

		void _discard_env_session(Id_space<Parent::Client>::Id);

		Close_result _close(Session_state &);

		/**
		 * Session_state::Ready_callback
		 */
		void session_ready(Session_state &session) override;

		/**
		 * Session_state::Closed_callback
		 */
		void session_closed(Session_state &) override;

		template <typename UNIT>
		static UNIT _effective_quota(UNIT requested_quota, UNIT env_quota)
		{
			if (requested_quota.value < env_quota.value)
				return UNIT { 0 };

			return UNIT { requested_quota.value - env_quota.value };
		}

	public:

		/**
		 * Constructor
		 *
		 * \param rm          local address space, usually 'env.rm()'
		 * \param entrypoint  entrypoint used to serve the parent interface of
		 *                    the child
		 * \param policy      policy for the child
		 *
		 * \throw Service_denied  the initial sessions for the child's
		 *                        environment could not be established
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
		 * Return true if the child has been started
		 *
		 * After the child's construction, the child is not always able to run
		 * immediately. In particular, a session of the child's environment
		 * may still be pending. This method returns true only if the child's
		 * environment is completely initialized at the time of calling.
		 *
		 * If all environment sessions are immediately available (as is the
		 * case for local services or parent services), the return value is
		 * expected to be true. If this is not the case, one of child's
		 * environment sessions could not be established, e.g., the ROM session
		 * of the binary could not be obtained.
		 */
		bool active() const { return _process.constructed(); }

		/**
		 * Initialize the child's PD session
		 */
		void initiate_env_pd_session();

		/**
		 * Trigger the routing and creation of the child's environment session
		 *
		 * See the description of 'Child_policy::initiate_env_sessions'.
		 */
		void initiate_env_sessions();

		/**
		 * Return true if the child is safe to be destroyed
		 *
		 * The child must not be destroyed until all environment sessions
		 * are closed at the respective servers. Otherwise, the session state,
		 * which is kept as part of the child object may be gone before
		 * the close request reaches the server.
		 */
		bool env_sessions_closed() const
		{
			if (_linker.constructed() && !_linker->closed()) return false;

			/*
			 * Note that the state of the CPU session remains unchecked here
			 * because the eager CPU-session destruction does not work on all
			 * kernels (search for KERNEL_SUPPORTS_EAGER_CHILD_DESTRUCTION).
			 */
			return _log.closed() && _binary.closed();
		}

		/**
		 * Quota unconditionally consumed by the child's environment
		 */
		static Ram_quota env_ram_quota()
		{
			return { Cpu_connection::RAM_QUOTA + Pd_connection::RAM_QUOTA +
			         Log_connection::RAM_QUOTA + 2*Rom_connection::RAM_QUOTA };
		}

		static Cap_quota env_cap_quota()
		{
			return { Cpu_connection::CAP_QUOTA + Pd_connection::CAP_QUOTA +
			         Log_connection::CAP_QUOTA + 2*Rom_connection::CAP_QUOTA +
			         1 /* parent cap */ };
		}

		void close_all_sessions();

		template <typename FN>
		void for_each_session(FN const &fn) const
		{
			_id_space.for_each<Session_state const>(fn);
		}

		/**
		 * Deduce env session costs from usable RAM quota
		 */
		static Ram_quota effective_quota(Ram_quota quota)
		{
			return _effective_quota(quota, env_ram_quota());
		}

		/**
		 * Deduce env session costs from usable cap quota
		 */
		static Cap_quota effective_quota(Cap_quota quota)
		{
			return _effective_quota(quota, env_cap_quota());
		}

		Pd_session_capability pd_session_cap() const { return _pd.cap();  }

		Parent_capability parent_cap() const { return cap(); }

		Ram_allocator       &ram()       { return _pd.session(); }
		Ram_allocator const &ram() const { return _pd.session(); }
		Cpu_session         &cpu()       { return _cpu.session(); }
		Pd_session          &pd()        { return _pd.session(); }
		Pd_session    const &pd()  const { return _pd.session(); }

		/**
		 * Request factory for creating session-state objects
		 */
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

		/**
		 * Notify the child to give a lifesign
		 */
		void heartbeat();

		/**
		 * Return number of missing heartbeats since the last response from
		 * the child
		 */
		unsigned skipped_heartbeats() const;


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
		void heartbeat_sigh(Signal_context_capability) override;
		void heartbeat_response() override;
};

#endif /* _INCLUDE__BASE__CHILD_H_ */
