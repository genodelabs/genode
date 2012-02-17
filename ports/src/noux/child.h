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
				struct Dataspace_info : Object_pool<Dataspace_info>::Entry
				{
					size_t               _size;
					Dataspace_capability _ds_cap;


					Dataspace_info(Dataspace_capability ds_cap)
					:
						Object_pool<Dataspace_info>::Entry(ds_cap),
						_size(Dataspace_client(ds_cap).size()),
						_ds_cap(ds_cap)
					{ }

					size_t                 size() const { return _size; }
					Dataspace_capability ds_cap() const { return _ds_cap; }
				};


				struct Local_ram_session : Rpc_object<Ram_session>
				{
					Object_pool<Dataspace_info> _pool;
					size_t                      _used_quota;

					/**
					 * Constructor
					 */
					Local_ram_session() : _used_quota(0) { }

					/**
					 * Destructor
					 */
					~Local_ram_session()
					{
						PWRN("~Local_ram_session not yet implemented, leaking RAM");
					}


					/***************************
					 ** Ram_session interface **
					 ***************************/

					Ram_dataspace_capability alloc(size_t size)
					{
						PINF("RAM alloc %zd", size);

						Ram_dataspace_capability ds_cap = env()->ram_session()->alloc(size);

						Dataspace_info *ds_info = new (env()->heap())
						                          Dataspace_info(ds_cap);

						_used_quota += ds_info->size();

						_pool.insert(ds_info);

						return ds_cap;
					}

					void free(Ram_dataspace_capability ds_cap)
					{
						PINF("RAM free");

						Dataspace_info *ds_info = _pool.obj_by_cap(ds_cap);

						if (!ds_info) {
							PERR("RAM free: dataspace lookup failed");
							return;
						}
						
						_pool.remove(ds_info);
						_used_quota -= ds_info->size();

						env()->ram_session()->free(ds_cap);
						destroy(env()->heap(), ds_info);
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
					Rm_connection rm;

					Local_addr attach(Dataspace_capability ds,
					                  size_t size = 0, off_t offset = 0,
					                  bool use_local_addr = false,
					                  Local_addr local_addr = (addr_t)0)
					{
						PINF("RM attach called");

						/*
						 * XXX look if we can identify the specified dataspace.
						 *     Is it a dataspace allocated via 'Local_ram_session'?
						 */

						return rm.attach(ds, size, offset, use_local_addr, local_addr);
					}

					void detach(Local_addr local_addr)
					{
						PINF("RM detach called");
						rm.detach(local_addr);
					}

					Pager_capability add_client(Thread_capability thread)
					{
						PINF("RM add client called");
						return rm.add_client(thread);
					}

					void fault_handler(Signal_context_capability handler)
					{
						PINF("RM fault handler called");
						return rm.fault_handler(handler);
					}

					State state()
					{
						PINF("RM state called");
						return rm.state();
					}

					Dataspace_capability dataspace()
					{
						PINF("RM dataspace called");
						return rm.dataspace();
					}
				};


				/**
				 * Used to defer the execution of the process' main thread
				 */
				struct Local_cpu_session : Rpc_object<Cpu_session>
				{
					bool forked;
					Cpu_connection cpu;

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
					 * Explicity start main thread, only meaningful when
					 * 'forked' is true
					 */
					void start_main_thread(addr_t ip, addr_t sp)
					{
						cpu.start(main_thread, ip, sp);
					}
				};

				Rpc_entrypoint ep;

				Local_ram_session      ram;
				Local_cpu_session      cpu;
				Local_rm_session       rm;

				Resources(char const *label, Rpc_entrypoint &ep, bool forked)
				:
					ep(ep),
					cpu(label, forked)
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
				_binary_ds(vfs->dataspace_from_file(name)),
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
				_local_noux_service(_noux_session_cap)
			{
				_args.dump();
				strncpy(_name, name, sizeof(_name));
			}

			~Child()
			{
				/* XXX _binary_ds */

				_entrypoint.dissolve(this);
			}

			void start() { _entrypoint.activate(); }

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
