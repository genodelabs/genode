/*
 * \brief  Noux child process
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
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
#include <noux_session/capability.h>
#include <args.h>
#include <environment.h>
#include <ram_session_component.h>
#include <cpu_session_component.h>
#include <child_policy.h>
#include <io_receptor_registry.h>
#include <destruct_queue.h>
#include <destruct_dispatcher.h>

#include <local_cpu_service.h>
#include <local_ram_service.h>
#include <local_rom_service.h>

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

	/**
	 * Return singleton instance of Io_receptor_registry
	 */
	Io_receptor_registry *io_receptor_registry();

	/**
	 * Return ELF binary of dynamic linker
	 */
	Dataspace_capability ldso_ds_cap();

	class Child;

	bool is_init_process(Child *child);
	void init_process_exited();

	class Child : public Rpc_object<Session>,
	              public File_descriptor_registry,
	              public Family_member,
	              public Destruct_queue::Element<Child>
	{
		private:

			Signal_receiver *_sig_rec;

			/**
			 * Semaphore used for implementing blocking syscalls, i.e., select
			 */
			Semaphore _blocker;

			Allocator                 *_alloc;
			Destruct_queue            &_destruct_queue;
			Destruct_dispatcher        _destruct_dispatcher;
			Signal_context_capability  _destruct_context_cap;

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

			/**
			 * ELF binary handling
			 */
			struct Elf
			{
				enum { NAME_MAX_LEN = 128 };
				char _name[NAME_MAX_LEN];

				Dir_file_system    * const _root_dir;
				Dataspace_capability const _binary_ds;

				Elf(char const * const binary_name, Dir_file_system * root_dir,
				    Dataspace_capability binary_ds)
				:
					_root_dir(root_dir), _binary_ds(binary_ds)
				{
					strncpy(_name, binary_name, sizeof(_name));
					_name[NAME_MAX_LEN - 1] = 0;
				}

				~Elf() { _root_dir->release(_name, _binary_ds); }
			} _elf;

			enum { PAGE_SIZE = 4096, PAGE_MASK = ~(PAGE_SIZE - 1) };
			enum { SYSIO_DS_SIZE = PAGE_MASK & (sizeof(Sysio) + PAGE_SIZE - 1) };

			Attached_ram_dataspace _sysio_ds;
			Sysio * const          _sysio;

			Session_capability const _noux_session_cap;

			Local_noux_service _local_noux_service;
			Local_ram_service  _local_ram_service;
			Local_cpu_service  _local_cpu_service;
			Local_rm_service   _local_rm_service;
			Local_rom_service  _local_rom_service;
			Service_registry  &_parent_services;

			Static_dataspace_info _binary_ds_info;
			Static_dataspace_info _sysio_ds_info;
			Static_dataspace_info _ldso_ds_info;
			Static_dataspace_info _args_ds_info;
			Static_dataspace_info _env_ds_info;

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

			Dir_file_system    * const root_dir() { return _elf._root_dir; }

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
			Child(char const        *binary_name,
			      Family_member     *parent,
			      int                pid,
			      Signal_receiver   *sig_rec,
			      Dir_file_system   *root_dir,
			      Args               const &args,
			      Sysio::Env         const &env,
			      Cap_session       *cap_session,
			      Service_registry  &parent_services,
			      Rpc_entrypoint    &resources_ep,
			      bool               forked,
			      Allocator         *destruct_alloc,
			      Destruct_queue    &destruct_queue,
			      bool               verbose)
			:
				Family_member(pid, parent),
				Destruct_queue::Element<Child>(destruct_alloc),
				_sig_rec(sig_rec),
				_destruct_queue(destruct_queue),
				_destruct_dispatcher(_destruct_queue, this),
				_destruct_context_cap(sig_rec->manage(&_destruct_dispatcher)),
				_cap_session(cap_session),
				_entrypoint(cap_session, STACK_SIZE, "noux_process", false),
				_resources(binary_name, resources_ep, false),
				_args(ARGS_DS_SIZE, args),
				_env(env),
				_elf(binary_name, root_dir, root_dir->dataspace(binary_name)),
				_sysio_ds(Genode::env()->ram_session(), SYSIO_DS_SIZE),
				_sysio(_sysio_ds.local_addr<Sysio>()),
				_noux_session_cap(Session_capability(_entrypoint.manage(this))),
				_local_noux_service(_noux_session_cap),
				_local_ram_service(_entrypoint),
				_local_cpu_service(_entrypoint, _resources.cpu.cpu_cap()),
				_local_rm_service(_entrypoint, _resources.ds_registry),
				_local_rom_service(_entrypoint, _resources.ds_registry),
				_parent_services(parent_services),
				_binary_ds_info(_resources.ds_registry, _elf._binary_ds),
				_sysio_ds_info(_resources.ds_registry, _sysio_ds.cap()),
				_ldso_ds_info(_resources.ds_registry, ldso_ds_cap()),
				_args_ds_info(_resources.ds_registry, _args.cap()),
				_env_ds_info(_resources.ds_registry, _env.cap()),
				_child_policy(_elf._name, _elf._binary_ds, _args.cap(), _env.cap(),
				              _entrypoint, _local_noux_service,
				              _local_rm_service, _local_rom_service,
				              _parent_services,
				              *this, *this, _destruct_context_cap,
				              _resources.ram, verbose),
				_child(forked ? Dataspace_capability() : _elf._binary_ds,
				       _resources.ram.cap(), _resources.cpu.cap(),
				       _resources.rm.cap(), &_entrypoint, &_child_policy,
				       /**
				        * Override the implicit assignment to _parent_service
				        */
				       _local_ram_service, _local_cpu_service, _local_rm_service)
			{
				if (verbose)
					_args.dump();

				if (!forked && !_elf._binary_ds.valid()) {
					PERR("Lookup of executable \"%s\" failed", binary_name);
					throw Binary_does_not_exist();
				}
			}

			~Child()
			{
				_sig_rec->dissolve(&_destruct_dispatcher);

				_entrypoint.dissolve(this);
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
				if (is_init_process(this)) {
					PINF("init process exited");

					/* trigger exit of main event loop */
					init_process_exited();
				} else {
					Signal_transmitter(_destruct_context_cap).submit();
				}
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

			int next_open_fd(int start_fd)
			{
				if (start_fd >= 0)
					for (int fd = start_fd; fd < MAX_FILE_DESCRIPTORS; fd++)
						if (fd_in_use(fd))
							return fd;
				return -1;
			}
	};
};

#endif /* _NOUX__CHILD_H_ */
