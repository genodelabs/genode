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
#include <base/signal.h>
#include <base/semaphore.h>
#include <cap_session/cap_session.h>
#include <os/attached_ram_dataspace.h>

/* Noux includes */
#include <file_descriptor_registry.h>
#include <dir_file_system.h>
#include <signal_dispatcher.h>
#include <noux_session/capability.h>
#include <args.h>
#include <environment.h>
#include <ram_session_component.h>
#include <cpu_session_component.h>
#include <child_policy.h>

extern void (*cleanup_socket_descriptors)();

namespace Noux {

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
	Pid_allocator *pid_allocator();

	/**
	 * Return singleton instance of timeout scheduler
	 */
	class Timeout_scheduler;
	Timeout_scheduler *timeout_scheduler();

	/**
	 * Return singleton instance of user information
	 */
	class User_info;
	User_info *user_info();


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
				} else {
					/* destroy 'Noux::Child' */
					destroy(env()->heap(), _child);

					PINF("destroy %p", _child);
					PINF("quota: avail=%zd, used=%zd",
					     env()->ram_session()->avail(),
					     env()->ram_session()->used());
				}
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
				destroy(env()->heap(), _child);
			}
	};


	class Child : public Rpc_object<Session>,
	              public File_descriptor_registry,
	              public Family_member
	{
		private:

			Signal_receiver *_sig_rec;

			/**
			 * Semaphore used for implementing blocking syscalls, i.e., select
			 */
			Semaphore _blocker;

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
				/**
				 * Entrypoint used to serve the RPC interfaces of the
				 * locally-provided services
				 */
				Rpc_entrypoint &ep;

				/**
				 * Registry of dataspaces owned by the Noux process
				 */
				Dataspace_registry ds_registry;

				/**
				 * Locally-provided services for accessing platform resources
				 */
				Ram_session_component ram;
				Cpu_session_component cpu;
				Rm_session_component  rm;

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

			Dir_file_system * const _root_dir;

			/**
			 * ELF binary
			 */
			Dataspace_capability const _binary_ds;

			enum { PAGE_SIZE = 4096, PAGE_MASK = ~(PAGE_SIZE - 1) };
			enum { SYSIO_DS_SIZE = PAGE_MASK & (sizeof(Sysio) + PAGE_SIZE - 1) };

			Attached_ram_dataspace _sysio_ds;
			Sysio * const          _sysio;

			Session_capability const _noux_session_cap;

			Local_noux_service _local_noux_service;
			Local_rm_service   _local_rm_service;
			Service_registry  &_parent_services;

			Child_policy  _child_policy;
			Genode::Child _child;

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

			void _block_for_io_channel(Shared_pointer<Io_channel> &io)
			{
				Wake_up_notifier notifier(&_blocker);
				io->register_wake_up_notifier(&notifier);
				_blocker.down();
				io->unregister_wake_up_notifier(&notifier);
			}

			/**
			 * Method for handling noux network related system calls
			 */

			bool _syscall_net(Syscall sc);

		public:

			struct Binary_does_not_exist : Exception { };

			/**
			 * Constructor
			 *
			 * \param forked  false if the child is spawned directly from
			 *                an executable binary (i.e., the init process,
			 *                or children created via execve, or
			 *                true if the child is a fork from another child
			 *
			 * \throw Binary_does_not_exist  if child is not a fork and the
			 *                               specified name could not be
			 *                               looked up at the virtual file
			 *                               system
			 */
			Child(char const       *name,
			      Family_member    *parent,
			      int               pid,
			      Signal_receiver  *sig_rec,
			      Dir_file_system  *root_dir,
			      Args              const &args,
			      Sysio::Env        const &env,
			      Cap_session      *cap_session,
			      Service_registry &parent_services,
			      Rpc_entrypoint   &resources_ep,
			      bool              forked)
			:
				Family_member(pid, parent),
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
				_root_dir(root_dir),
				_binary_ds(forked ? Dataspace_capability()
				                  : root_dir->dataspace(name)),
				_sysio_ds(Genode::env()->ram_session(), SYSIO_DS_SIZE),
				_sysio(_sysio_ds.local_addr<Sysio>()),
				_noux_session_cap(Session_capability(_entrypoint.manage(this))),
				_local_noux_service(_noux_session_cap),
				_local_rm_service(_entrypoint, _resources.ds_registry),
				_parent_services(parent_services),
				_child_policy(name, _binary_ds, _args.cap(), _env.cap(),
				              _entrypoint, _local_noux_service,
				              _local_rm_service, _parent_services,
				              *this, *this, _exit_context_cap, _resources.ram),
				_child(_binary_ds, _resources.ram.cap(), _resources.cpu.cap(),
				       _resources.rm.cap(), &_entrypoint, &_child_policy)
			{
				_args.dump();

				if (!forked && !_binary_ds.valid()) {
					PERR("Lookup of executable \"%s\" failed", name);
					throw Binary_does_not_exist();
				}
			}

			~Child()
			{
				/* short-cut to close all remaining open sd's if the child exits */
				if (cleanup_socket_descriptors)
					cleanup_socket_descriptors();

				_sig_rec->dissolve(&_execve_cleanup_dispatcher);
				_sig_rec->dissolve(&_exit_dispatcher);

				_entrypoint.dissolve(this);

				_root_dir->release(_child_policy.name(), _binary_ds);
			}

			void start() { _entrypoint.activate(); }

			void start_forked_main_thread(addr_t ip, addr_t sp, addr_t parent_cap_addr)
			{
				/* poke parent_cap_addr into child's address space */
				Capability<Parent> const &cap = _child.parent_cap();
				Capability<Parent>::Raw   raw = { cap.dst(), cap.local_name() };
				_resources.rm.poke(parent_cap_addr, &raw, sizeof(raw));

				/* start execution of new main thread at supplied trampoline */
				_resources.cpu.start_main_thread(ip, sp);
			}

			void submit_exit_signal()
			{
				Signal_transmitter(_exit_context_cap).submit();
			}

			Ram_session_capability ram() const { return _resources.ram.cap(); }
			Rm_session_capability   rm() const { return _resources.rm.cap(); }
			Dataspace_registry &ds_registry()  { return _resources.ds_registry; }


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
