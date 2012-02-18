/*
 * \brief  Noux child process
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__CHILD_H_
#define _NOUX__CHILD_H_

/* Genode includes */
#include <init/child_policy.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>
#include <base/signal.h>
#include <base/semaphore.h>
#include <cap_session/cap_session.h>
#include <os/attached_ram_dataspace.h>

/* Noux includes */
#include <file_descriptor_registry.h>
#include <vfs.h>
#include <signal_dispatcher.h>
#include <noux_session/capability.h>
#include <args.h>
#include <environment.h>

namespace Noux {

	using namespace Genode;


	/**
	 * Allocator for process IDs
	 */
	class Pid_allocator
	{
		private:

			Lock _lock;
			int  _num_pids;

		public:

			Pid_allocator() : _num_pids(0) { }

			int alloc()
			{
				Lock::Guard guard(_lock);
				return _num_pids++;
			}
	};


	/**
	 * Return singleton instance of PID allocator
	 */
	Pid_allocator *pid_allocator()
	{
		static Pid_allocator inst;
		return &inst;
	}


	class Child;

	bool is_init_process(Child *child);
	void init_process_exited();


	/**
	 * Signal context used for child exit
	 */
	class Child_exit_dispatcher : public Signal_dispatcher
	{
		private:

			Child *_child;

		public:

			Child_exit_dispatcher(Child *child) : _child(child) { }

			void dispatch()
			{
				if (is_init_process(_child)) {
					PINF("init process exited");

					/* trigger exit of main event loop */
					init_process_exited();
				}
				/* XXX destroy child */
			}
	};


	/**
	 * Signal context used for removing the child after having executed 'execve'
	 */
	class Child_execve_cleanup_dispatcher : public Signal_dispatcher
	{
		private:

			Child *_child;

		public:

			Child_execve_cleanup_dispatcher(Child *child) : _child(child) { }

			void dispatch()
			{
				PINF("execve cleanup dispatcher called");
				destroy(env()->heap(), _child);
			}
	};


	class Child : private Child_policy,
	              public  Rpc_object<Session>,
	              public  File_descriptor_registry
	{
		private:

			int const _pid;

			Signal_receiver *_sig_rec;

			/**
			 * Semaphore used for implementing blocking syscalls, i.e., select
			 */
			Semaphore _blocker;

			enum { MAX_NAME_LEN = 64 };
			char _name[MAX_NAME_LEN];

			Child_exit_dispatcher     _exit_dispatcher;
			Signal_context_capability _exit_context_cap;

			Child_execve_cleanup_dispatcher _execve_cleanup_dispatcher;
			Signal_context_capability       _execve_cleanup_context_cap;

			Cap_session * const _cap_session;

			enum { STACK_SIZE = 4*1024*sizeof(long) };
			Rpc_entrypoint _entrypoint;

			/**
			 * Resources assigned to the child
			 */
			struct Resources
			{
				/*
				 * XXX not used yet
				 */
				struct Dataspace_destroyer
				{
					virtual void destroy(Dataspace_capability) = 0;
				};

				class Dataspace_info : public Object_pool<Dataspace_info>::Entry
				{
					private:

						size_t               _size;
						Dataspace_capability _ds_cap;
						Dataspace_destroyer &_destroyer;

					public:

						Dataspace_info(Dataspace_capability ds_cap,
						               Dataspace_destroyer &destroyer)
						:
							Object_pool<Dataspace_info>::Entry(ds_cap),
							_size(Dataspace_client(ds_cap).size()),
							_ds_cap(ds_cap),
							_destroyer(destroyer)
						{ }

						size_t                 size() const { return _size; }
						Dataspace_capability ds_cap() const { return _ds_cap; }

						virtual Dataspace_capability fork(Ram_session_capability ram) = 0;

						Dataspace_destroyer &destroyer() { return _destroyer; }
				};


				class Dataspace_registry
				{
					private:

						Object_pool<Dataspace_info> _pool;

					public:

						void insert(Dataspace_info *info)
						{
							_pool.insert(info);
						}

						void remove(Dataspace_info *info)
						{
							_pool.remove(info);
						}

						Dataspace_info *lookup_info(Dataspace_capability ds_cap)
						{
							return _pool.obj_by_cap(ds_cap);
						}
				};


				struct Ram_dataspace_info : Dataspace_info,
				                            List<Ram_dataspace_info>::Element
				{
					Ram_dataspace_info(Ram_dataspace_capability ds_cap,
					                   Dataspace_destroyer &destroyer)
					: Dataspace_info(ds_cap, destroyer) { }

					Dataspace_capability fork(Ram_session_capability ram)
					{
						size_t const size = Dataspace_client(ds_cap()).size();

						Ram_dataspace_capability dst_ds;

						try {
							dst_ds = Ram_session_client(ram).alloc(size);
						} catch (...) {
							return Ram_dataspace_capability();
						}

						void *src = 0;
						try {
							src = Genode::env()->rm_session()->attach(ds_cap());
						} catch (...) { }

						void *dst = 0;
						try {
							dst = Genode::env()->rm_session()->attach(dst_ds);
						} catch (...) { }

						if (src && dst)
							memcpy(dst, src, size);

						if (src) Genode::env()->rm_session()->detach(src);
						if (dst) Genode::env()->rm_session()->detach(dst);

						if (!src || !dst) {
							Ram_session_client(ram).free(dst_ds);
							return Ram_dataspace_capability();
						}
						
						return dst_ds;
					}
				};


				struct Local_ram_session : Rpc_object<Ram_session>,
				                           Dataspace_destroyer
				{
					List<Ram_dataspace_info> _list;

					/*
					 * Track the RAM resources accumulated via RAM session
					 * allocations.
					 */
					size_t _used_quota;

					Dataspace_registry &_registry;

					/**
					 * Constructor
					 */
					Local_ram_session(Dataspace_registry &registry)
					: _used_quota(0), _registry(registry) { }

					/**
					 * Destructor
					 */
					~Local_ram_session()
					{
						Ram_dataspace_info *info = 0;
						while ((info = _list.first()))
							free(static_cap_cast<Ram_dataspace>(info->ds_cap()));
					}


					/***********************************
					 ** Dataspace_destroyer interface **
					 ***********************************/

					/* XXX not yet used */
					void destroy(Dataspace_capability ds)
					{
						free(static_cap_cast<Ram_dataspace>(ds));
					}


					/***************************
					 ** Ram_session interface **
					 ***************************/

					Ram_dataspace_capability alloc(size_t size)
					{
						Ram_dataspace_capability ds_cap = env()->ram_session()->alloc(size);

						Ram_dataspace_info *ds_info = new (env()->heap())
						                              Ram_dataspace_info(ds_cap, *this);

						_used_quota += ds_info->size();

						_registry.insert(ds_info);
						_list.insert(ds_info);

						return ds_cap;
					}

					void free(Ram_dataspace_capability ds_cap)
					{
						Ram_dataspace_info *ds_info =
							dynamic_cast<Ram_dataspace_info *>(_registry.lookup_info(ds_cap));

						if (!ds_info) {
							PERR("RAM free: dataspace lookup failed");
							return;
						}
						
						_registry.remove(ds_info);
						_list.remove(ds_info);
						_used_quota -= ds_info->size();

						env()->ram_session()->free(ds_cap);
						Genode::destroy(env()->heap(), ds_info);
					}

					int ref_account(Ram_session_capability) { return 0; }
					int transfer_quota(Ram_session_capability, size_t) { return 0; }
					size_t quota() { return env()->ram_session()->quota(); }
					size_t used() { return _used_quota; }
				};


				/**
				 * Used to record all RM attachements
				 */
				struct Local_rm_session : Rpc_object<Rm_session>
				{
					struct Region : List<Region>::Element
					{
						Dataspace_capability ds;
						size_t               size;
						off_t                offset;
						addr_t               local_addr;

						Region(Dataspace_capability ds, size_t size,
						       off_t offset, addr_t local_addr)
						:
							ds(ds), size(size),
							offset(offset), local_addr(local_addr)
						{ }

						/**
						 * Return true if region contains specified address
						 */
						bool contains(addr_t addr) const
						{
							return (addr >= local_addr)
							    && (addr <  local_addr + size);
						}
					};

					Rm_connection _rm;

					Dataspace_registry &_ds_registry;

					List<Region> _regions;

					Region *_lookup_region_by_addr(addr_t local_addr)
					{
						Region *curr = _regions.first();
						for (; curr; curr = curr->next()) {
							if (curr->contains(local_addr))
							    return curr;
						}
						return 0;
					}

					Local_rm_session(Dataspace_registry &ds_registry,
					                 addr_t start = ~0UL, size_t size = 0)
					: _rm(start, size), _ds_registry(ds_registry) { }

					~Local_rm_session()
					{
						Region *curr = 0;
						for (; curr; curr = curr->next())
							detach(curr->local_addr);
					}

					/**
					 * Replay attachments onto specified RM session
					 *
					 * \param dst_ram  backing store used for allocating the
					 *                 the copies of RAM dataspaces
					 */
					void replay(Ram_session_capability dst_ram,
					            Rm_session_capability  dst_rm)
					{
						Region *curr = _regions.first();
						for (; curr; curr = curr->next()) {

							Dataspace_capability ds;

							Dataspace_info *info = _ds_registry.lookup_info(curr->ds);

							if (info) {
								ds = info->fork(dst_ram);
								if (!ds.valid())
									PERR("replay: Error while forking dataspace");

								/*
								 * XXX We could detect dataspaces that are
								 *     attached more than once. For now, we
								 *     create a new fork for each attachment.
								 */

							} else {

								/*
								 * If the dataspace is not a RAM dataspace,
								 * assume that it's a ROM dataspace.
								 *
								 * XXX Handle ROM dataspaces explicitly. For
								 *     once, we need to make sure that they
								 *     remain available until the child process
								 *     exits even if the parent process exits
								 *     earlier. Furthermore, we would like to
								 *     detect unexpected dataspaces.
								 */
								ds = curr->ds;
								PWRN("replay: unknown dataspace type");
							}

							Rm_session_client(dst_rm).attach(ds, curr->size,
							                                 curr->offset,
							                                 true,
							                                 curr->local_addr);
						}
					}


					/**************************
					 ** RM session interface **
					 **************************/

					Local_addr attach(Dataspace_capability ds,
					                  size_t size = 0, off_t offset = 0,
					                  bool use_local_addr = false,
					                  Local_addr local_addr = (addr_t)0)
					{
						if (size == 0)
							size = Dataspace_client(ds).size();

						/*
						 * XXX look if we can identify the specified dataspace.
						 *     Is it a dataspace allocated via 'Local_ram_session'?
						 */

						local_addr = _rm.attach(ds, size, offset,
						                        use_local_addr, local_addr);

						/*
						 * Record attachement for later replay (needed during
						 * fork)
						 */
						_regions.insert(new (Genode::env()->heap())
						                Region(ds, size, offset, local_addr));
						return local_addr;
					}

					void detach(Local_addr local_addr)
					{
						_rm.detach(local_addr);

						Region *region = _lookup_region_by_addr(local_addr);
						if (!region) {
							PWRN("Attempt to detach unknown region at 0x%p",
							     (void *)local_addr);
							return;
						}

						_regions.remove(region);
						destroy(Genode::env()->heap(), region);
					}

					Pager_capability add_client(Thread_capability thread)
					{
						return _rm.add_client(thread);
					}

					void fault_handler(Signal_context_capability handler)
					{
						return _rm.fault_handler(handler);
					}

					State state()
					{
						return _rm.state();
					}

					Dataspace_capability dataspace()
					{
						return _rm.dataspace();
					}
				};


				struct Sub_rm_dataspace_info : Dataspace_info
				{
					struct Dummy_destroyer : Dataspace_destroyer
					{
						void destroy(Dataspace_capability) { }
					} _dummy_destroyer;

					Local_rm_session &_sub_rm;

					Sub_rm_dataspace_info(Local_rm_session &sub_rm)
					:
						Dataspace_info(sub_rm.dataspace(), _dummy_destroyer),
						_sub_rm(sub_rm)
					{ }

					Dataspace_capability fork(Ram_session_capability ram)
					{
						Rm_connection *new_sub_rm = new Rm_connection(0, size());

						/*
						 * XXX We are leaking the 'Rm_connection' because of
						 *     keeping only the dataspace.
						 */

						_sub_rm.replay(ram, new_sub_rm->cap());

						Dataspace_capability ds_cap = new_sub_rm->dataspace();

						return ds_cap;
					}
				};


				/**
				 * Used to defer the execution of the process' main thread
				 */
				struct Local_cpu_session : Rpc_object<Cpu_session>
				{
					bool const        forked;
					Cpu_connection    cpu;
					Thread_capability main_thread;

					Local_cpu_session(char const *label, bool forked)
					: forked(forked), cpu(label) { }

					Thread_capability create_thread(Name const &name)
					{
						/*
						 * Prevent any attempt to create more than the main
						 * thread.
						 */
						if (main_thread.valid()) {
							PWRN("Invalid attempt to create a thread besides main");
							return Thread_capability();
						}
						main_thread = cpu.create_thread(name);

						PINF("created main thread");
						return main_thread;
					}

					void kill_thread(Thread_capability thread) {
						cpu.kill_thread(thread); }

					Thread_capability first() {
						return cpu.first(); }

					Thread_capability next(Thread_capability curr) {
						return cpu.next(curr); }

					int set_pager(Thread_capability thread,
					              Pager_capability  pager) {
					    return cpu.set_pager(thread, pager); }

					int start(Thread_capability thread, addr_t ip, addr_t sp)
					{
						if (forked) {
							PINF("defer attempt to start thread at ip 0x%lx", ip);
							return 0;
						}
						return cpu.start(thread, ip, sp);
					}

					void pause(Thread_capability thread) {
						cpu.pause(thread); }

					void resume(Thread_capability thread) {
						cpu.resume(thread); }

					void cancel_blocking(Thread_capability thread) {
						cpu.cancel_blocking(thread); }

					int state(Thread_capability thread, Thread_state *dst) {
						return cpu.state(thread, dst); }

					void exception_handler(Thread_capability         thread,
					                       Signal_context_capability handler) {
					    cpu.exception_handler(thread, handler); }

					void single_step(Thread_capability thread, bool enable) {
						cpu.single_step(thread, enable); }

					/**
					 * Explicitly start main thread, only meaningful when
					 * 'forked' is true
					 */
					void start_main_thread(addr_t ip, addr_t sp)
					{
						cpu.start(main_thread, ip, sp);
					}
				};

				Rpc_entrypoint    &ep;
				Dataspace_registry ds_registry;
				Local_ram_session  ram;
				Local_cpu_session  cpu;
				Local_rm_session   rm;

				Resources(char const *label, Rpc_entrypoint &ep, bool forked)
				:
					ep(ep), ram(ds_registry), cpu(label, forked), rm(ds_registry)
				{
					ep.manage(&ram);
					ep.manage(&rm);
					ep.manage(&cpu);
				}

				~Resources()
				{
					ep.dissolve(&ram);
					ep.dissolve(&rm);
					ep.dissolve(&cpu);
				}

			} _resources;

			/**
			 * Command line arguments
			 */
			Args_dataspace _args;

			/**
			 * Environment variables
			 */
			Environment _env;

			Vfs * const _vfs;

			/**
			 * ELF binary
			 */
			Dataspace_capability const _binary_ds;

			Genode::Child _child;

			Service_registry * const _parent_services;

			Init::Child_policy_enforce_labeling _labeling_policy;
			Init::Child_policy_provide_rom_file _binary_policy;
			Init::Child_policy_provide_rom_file _args_policy;
			Init::Child_policy_provide_rom_file _env_policy;

			enum { PAGE_SIZE = 4096, PAGE_MASK = ~(PAGE_SIZE - 1) };
			enum { SYSIO_DS_SIZE = PAGE_MASK & (sizeof(Sysio) + PAGE_SIZE - 1) };

			Attached_ram_dataspace _sysio_ds;
			Sysio * const          _sysio;

			Session_capability const _noux_session_cap;

			struct Local_noux_service : public Service
			{
				Genode::Session_capability _cap;

				/**
				 * Constructor
				 *
				 * \param cap  capability to return on session requests
				 */
				Local_noux_service(Genode::Session_capability cap)
				: Service(service_name()), _cap(cap) { }

				Genode::Session_capability session(const char *args) { return _cap; }
				void upgrade(Genode::Session_capability, const char *args) { }
				void close(Genode::Session_capability) { }

			} _local_noux_service;

			struct Local_sub_rm_service : public Service
			{
				Rpc_entrypoint                &_ep;
				Resources::Dataspace_registry &_ds_registry;

				Local_sub_rm_service(Rpc_entrypoint &ep,
				                     Resources::Dataspace_registry &ds_registry)
				:
					Service(Rm_session::service_name()),
					_ep(ep), _ds_registry(ds_registry)
				{ }

				Genode::Session_capability session(const char *args)
				{
					addr_t start = Arg_string::find_arg(args, "start").ulong_value(~0UL);
					size_t size  = Arg_string::find_arg(args, "size").ulong_value(0);

					Resources::Local_rm_session *rm =
						new Resources::Local_rm_session(_ds_registry, start, size);

					Genode::Session_capability cap = _ep.manage(rm);

					_ds_registry.insert(new Resources::Sub_rm_dataspace_info(*rm));

					return cap;
				}

				void upgrade(Genode::Session_capability, const char *args) { }

				void close(Genode::Session_capability)
				{
					PWRN("Local_sub_rm_service::close not implemented, leaking memory");
				}

			} _local_sub_rm_service;

			/**
			 * Exception type for failed file-descriptor lookup
			 */
			class Invalid_fd { };

			Shared_pointer<Io_channel> _lookup_channel(int fd) const
			{
				Shared_pointer<Io_channel> channel = io_channel_by_fd(fd);

				if (channel)
					return channel;

				throw Invalid_fd();
			}

			enum { ARGS_DS_SIZE = 4096 };

			/**
			 * Let specified child inherit our file descriptors
			 */
			void _assign_io_channels_to(Child *child)
			{
				for (int fd = 0; fd < MAX_FILE_DESCRIPTORS; fd++)
					if (fd_in_use(fd))
						child->add_io_channel(io_channel_by_fd(fd), fd);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param forked  false if the child is spawned directly from
			 *                an executable binary (i.e., the init process,
			 *                or children created via execve, or
			 *                true if the child is a fork from another child
			 */
			Child(char const        *name,
			      int                pid,
			      Signal_receiver   *sig_rec,
			      Vfs               *vfs,
			      Args               const &args,
			      char const        *env,
			      Cap_session       *cap_session,
			      Service_registry  *parent_services,
			      Rpc_entrypoint    &resources_ep,
			      bool               forked)
			:
				_pid(pid),
				_sig_rec(sig_rec),
				_exit_dispatcher(this),
				_exit_context_cap(sig_rec->manage(&_exit_dispatcher)),
				_execve_cleanup_dispatcher(this),
				_execve_cleanup_context_cap(sig_rec->manage(&_execve_cleanup_dispatcher)),
				_cap_session(cap_session),
				_entrypoint(cap_session, STACK_SIZE, "noux_process", false),
				_resources(name, resources_ep, false),
				_args(ARGS_DS_SIZE, args),
				_env(env),
				_vfs(vfs),
				_binary_ds(forked ? Dataspace_capability() 
				                  : vfs->dataspace_from_file(name)),
				_child(_binary_ds, _resources.ram.cap(), _resources.cpu.cap(),
				       _resources.rm.cap(), &_entrypoint, this),
				_parent_services(parent_services),
				_labeling_policy(_name),
				_binary_policy("binary", _binary_ds,  &_entrypoint),
				_args_policy(  "args",   _args.cap(), &_entrypoint),
				_env_policy(   "env",    _env.cap(),  &_entrypoint),
				_sysio_ds(Genode::env()->ram_session(), SYSIO_DS_SIZE),
				_sysio(_sysio_ds.local_addr<Sysio>()),
				_noux_session_cap(Session_capability(_entrypoint.manage(this))),
				_local_noux_service(_noux_session_cap),
				_local_sub_rm_service(_entrypoint, _resources.ds_registry)
			{
				_args.dump();
				strncpy(_name, name, sizeof(_name));

				Native_capability cap = _child.parent_cap();

				PINF("PID %d has parent cap %lx,%lx",
				     _pid, cap.tid().raw, cap.local_name());
			}

			~Child()
			{
				/* XXX _binary_ds */

				_entrypoint.dissolve(this);
			}

			void start() { _entrypoint.activate(); }

			void start_forked_main_thread(addr_t ip, addr_t sp, addr_t /* parent_cap_addr */)
			{
				/*
				 * XXX poke parent_cap_addr into child's address space
				 */
				_resources.cpu.start_main_thread(ip, sp);
			}

			Ram_session_capability ram() const { return _resources.ram.cap(); }
			Rm_session_capability   rm() const { return _resources.rm.cap(); }


			/****************************
			 ** Child_policy interface **
			 ****************************/

			const char *name() const { return _name; }

			Service *resolve_session_request(const char *service_name,
			                                 const char *args)
			{
				Service *service = 0;

				/* check for local ROM file requests */
				if ((service =   _args_policy.resolve_session_request(service_name, args))
				 || (service =    _env_policy.resolve_session_request(service_name, args))
				 || (service = _binary_policy.resolve_session_request(service_name, args)))
					return service;

				/* check for locally implemented noux service */
				if (strcmp(service_name, Session::service_name()) == 0)
					return &_local_noux_service;

				/*
				 * Check for the creation of an RM session, which is used by
				 * the dynamic linker to manually manage a part of the address
				 * space.
				 */
				if (strcmp(service_name, Rm_session::service_name()) == 0)
					return &_local_sub_rm_service;

				return _parent_services->find(service_name);
			}

			void filter_session_args(const char *service,
			                         char *args, size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
			}

			void exit(int exit_value)
			{
				PINF("child %s exited with exit value %d", _name, exit_value);
				Signal_transmitter(_exit_context_cap).submit();
			}

			Ram_session *ref_ram_session()
			{
				return &_resources.ram;
			}


			/****************************
			 ** Noux session interface **
			 ****************************/

			Dataspace_capability sysio_dataspace()
			{
				return _sysio_ds.cap();
			}

			bool syscall(Syscall sc);
	};
};

#endif /* _NOUX__CHILD_H_ */
