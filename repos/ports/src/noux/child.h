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
#include <pd_session/connection.h>
#include <os/attached_ram_dataspace.h>
#include <os/attached_rom_dataspace.h>

/* Noux includes */
#include <file_descriptor_registry.h>
#include <vfs/dir_file_system.h>
#include <noux_session/capability.h>
#include <args.h>
#include <environment.h>
#include <ram_session_component.h>
#include <cpu_session_component.h>
#include <pd_session_component.h>
#include <child_policy.h>
#include <io_receptor_registry.h>
#include <destruct_queue.h>
#include <destruct_dispatcher.h>
#include <interrupt_handler.h>
#include <kill_broadcaster.h>
#include <parent_execve.h>
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

	/*
	 * Return lock for protecting the signal queue
	 */
	Genode::Lock &signal_lock();

	class Child;

	/**
	 * Return true is child is the init process
	 */
	bool init_process(Child *child);
	void init_process_exited(int);

	struct Child_config : Attached_ram_dataspace
	{
		enum { CONFIG_DS_SIZE = 4096 };

		Child_config(Genode::Ram_session &ram)
		: Attached_ram_dataspace(&ram, CONFIG_DS_SIZE)
		{
			Genode::strncpy(local_addr<char>(),
			                "<config/>",
			                CONFIG_DS_SIZE);

			try {
				Attached_rom_dataspace noux_config("config");

				if (noux_config.xml().attribute_value("ld_verbose", false))
					Genode::strncpy(local_addr<char>(),
					                "<config ld_verbose=\"yes\"/>",
					                CONFIG_DS_SIZE);
			} catch (Genode::Rom_connection::Rom_connection_failed) { }
		}
	};

	class Child : public Rpc_object<Session>,
	              public File_descriptor_registry,
	              public Family_member,
	              public Destruct_queue::Element<Child>,
	              public Interrupt_handler
	{
		private:

			Child_policy::Name const _name;

			Parent_exit      *_parent_exit;
			Kill_broadcaster &_kill_broadcaster;
			Parent_execve    &_parent_execve;

			Signal_receiver &_sig_rec;

			Vfs::Dir_file_system &_root_dir;

			Destruct_queue            &_destruct_queue;
			Destruct_dispatcher        _destruct_dispatcher { _destruct_queue, this };
			Signal_context_capability  _destruct_context_cap =
				_sig_rec.manage(&_destruct_dispatcher);

			Pd_session &_env_pd_session; /* used for creating 'Rpc_entrypoint' */

			/**
			 * Entrypoint used to serve the RPC interfaces of the
			 * locally-provided services
			 */
			enum { STACK_SIZE = 8*1024*sizeof(long) };
			Rpc_entrypoint _ep { &_env_pd_session, STACK_SIZE, "noux_process", false };

			Ram_session                 &_ref_ram;
			Ram_session_capability const _ref_ram_cap;

			/**
			 * Registry of dataspaces owned by the Noux process
			 */
			Dataspace_registry _ds_registry;

			/**
			 * Locally-provided PD service
			 */
			typedef Local_service<Pd_session_component> Pd_service;
			Pd_session_component _pd { _ep, _name, _ds_registry };
			Pd_service::Single_session_factory _pd_factory { _pd };
			Pd_service                         _pd_service { _pd_factory };

			/**
			 * Locally-provided RAM service
			 */
			typedef Local_service<Ram_session_component> Ram_service;
			Ram_session_component _ram { _ep, _ds_registry };
			Ram_service::Single_session_factory _ram_factory { _ram };
			Ram_service                         _ram_service { _ram_factory };

			/**
			 * Locally-provided CPU service
			 */
			typedef Local_service<Cpu_session_component> Cpu_service;
			Cpu_session_component _cpu { _ep, _name, false, _ds_registry };
			Cpu_service::Single_session_factory _cpu_factory { _cpu };
			Cpu_service                         _cpu_service { _cpu_factory };

			/*
			 * Locally-provided Noux service
			 */
			Session_capability const _noux_session_cap = 
				Session_capability(_ep.manage(this));

			typedef Local_service<Rpc_object<Session> > Noux_service;
			Noux_service::Single_session_factory _noux_factory { *this };
			Noux_service                         _noux_service { _noux_factory };

			/*
			 * Locally-provided ROM service
			 */
			Local_rom_factory _rom_factory { _ep, _root_dir, _ds_registry };
			Local_rom_service _rom_service { _rom_factory };

			/**
			 * Command line arguments
			 */
			Args_dataspace _args;

			/**
			 * Environment variables
			 */
			Environment _env;

			/*
			 * Child configuration
			 */
			Child_config _config { *env()->ram_session() };

			enum { PAGE_SIZE = 4096, PAGE_MASK = ~(PAGE_SIZE - 1) };
			enum { SYSIO_DS_SIZE = PAGE_MASK & (sizeof(Sysio) + PAGE_SIZE - 1) };

			Attached_ram_dataspace _sysio_ds { env()->ram_session(), SYSIO_DS_SIZE };
			Sysio &_sysio = *_sysio_ds.local_addr<Sysio>();

			typedef Ring_buffer<enum Sysio::Signal, Sysio::SIGNAL_QUEUE_SIZE>
			        Signal_queue;
			Signal_queue _pending_signals;

			Parent_services &_parent_services;

			Static_dataspace_info _sysio_ds_info;
			Static_dataspace_info _args_ds_info;
			Static_dataspace_info _env_ds_info;
			Static_dataspace_info _config_ds_info;

			Child_policy _child_policy;

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

			/**
			 * Block until the IO channel is ready for reading or writing or an
			 * exception occured.
			 *
			 * \param io  the IO channel
			 * \param rd  check for data available for reading
			 * \param wr  check for readiness for writing
			 * \param ex  check for exceptions
			 */
			void _block_for_io_channel(Shared_pointer<Io_channel> &io,
			                           bool rd, bool wr, bool ex)
			{
				/* reset the blocker lock to the 'locked' state */
				_blocker.unlock();
				_blocker.lock();

				Wake_up_notifier notifier(&_blocker);
				io->register_wake_up_notifier(&notifier);

				for (;;) {
					if (io->check_unblock(rd, wr, ex) ||
					    !_pending_signals.empty())
						break;

					/* block (unless the lock got unlocked in the meantime) */
					_blocker.lock();
				}

				io->unregister_wake_up_notifier(&notifier);
			}

			/**
			 * Method for handling noux network related system calls
			 */
			bool _syscall_net(Syscall sc);

			void _destruct() {

				_sig_rec.dissolve(&_destruct_dispatcher);

				_ep.dissolve(this);

				if (init_process(this))
					init_process_exited(_child_policy.exit_value());
			}

		public:

			struct Binary_does_not_exist : Exception { };
			struct Insufficient_memory : Exception { };

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
			 * \throw Insufficent_memory if the child could not be started by
			 *                           the parent
			 */
			Child(Child_policy::Name const &name,
			      Parent_exit              *parent_exit,
			      Kill_broadcaster         &kill_broadcaster,
			      Parent_execve            &parent_execve,
			      int                       pid,
			      Signal_receiver          &sig_rec,
			      Vfs::Dir_file_system     &root_dir,
			      Args               const &args,
			      Sysio::Env         const &env,
			      Pd_session               &env_pd_session,
			      Ram_session              &ref_ram,
			      Ram_session_capability    ref_ram_cap,
			      Parent_services          &parent_services,
			      Rpc_entrypoint           &resources_ep,
			      bool                      forked,
			      Allocator                &destruct_alloc,
			      Destruct_queue           &destruct_queue,
			      bool                      verbose)
			:
				Family_member(pid),
				Destruct_queue::Element<Child>(&destruct_alloc),
				_name(name),
				_parent_exit(parent_exit),
				_kill_broadcaster(kill_broadcaster),
				_parent_execve(parent_execve),
				_sig_rec(sig_rec),
				_root_dir(root_dir),
				_destruct_queue(destruct_queue),
				_env_pd_session(env_pd_session),
				_ref_ram(ref_ram), _ref_ram_cap(ref_ram_cap),
				_args(ARGS_DS_SIZE, args),
				_env(env),
				_parent_services(parent_services),
				_sysio_ds_info(_ds_registry, _sysio_ds.cap()),
				_args_ds_info(_ds_registry, _args.cap()),
				_env_ds_info(_ds_registry, _env.cap()),
				_config_ds_info(_ds_registry, _config.cap()),
				_child_policy(name,
				              forked ? Rom_session_component::forked_magic_binary_name()
				                     : name,
				              _args.cap(), _env.cap(), _config.cap(),
				              _ep, _pd_service, _ram_service, _cpu_service,
				              _noux_service, _rom_service, _parent_services,
				              *this, parent_exit, *this, _destruct_context_cap,
				              ref_ram, ref_ram_cap, verbose),
				_child(*Genode::env()->rm_session(), _ep, _child_policy)
			{
				if (verbose)
					_args.dump();

				if (!_child.main_thread_cap().valid()) {
					_destruct();
					throw Insufficient_memory();
				}
			}

			~Child() { _destruct(); }

			void start() { _ep.activate(); }

			void start_forked_main_thread(addr_t ip, addr_t sp, addr_t parent_cap_addr)
			{
				/* poke parent_cap_addr into child's address space */
				Capability<Parent>::Raw const raw = _child.parent_cap().raw();

				_pd.poke(parent_cap_addr, &raw, sizeof(raw));

				/* start execution of new main thread at supplied trampoline */
				_cpu.start_main_thread(ip, sp);
			}

			void submit_exit_signal()
			{
				if (init_process(this)) {
					log("init process exited");

					/* trigger exit of main event loop */
					init_process_exited(_child_policy.exit_value());
				} else {
					Signal_transmitter(_destruct_context_cap).submit();
				}
			}

			Ram_session_component &ram() { return _ram; }
			Pd_session_component  &pd()  { return _pd;  }

			Dataspace_registry &ds_registry()  { return _ds_registry; }


			/****************************
			 ** Noux session interface **
			 ****************************/

			Dataspace_capability sysio_dataspace()
			{
				return _sysio_ds.cap();
			}

			Capability<Region_map> lookup_region_map(addr_t const addr)
			{
				return _pd.lookup_region_map(addr);
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


			/****************************************
			 ** File_descriptor_registry overrides **
			 ****************************************/

			/**
			 * Find out if the IO channel associated with 'fd' has more file
			 * descriptors associated with it
			 */
			bool _is_the_only_fd_for_io_channel(int fd,
			                                    Shared_pointer<Io_channel> io_channel)
			{
				for (int f = 0; f < MAX_FILE_DESCRIPTORS; f++) {
					if ((f != fd) &&
					    fd_in_use(f) &&
					    (io_channel_by_fd(f) == io_channel))
					return false;
				}

				return true;
			}

			int add_io_channel(Shared_pointer<Io_channel> io_channel, int fd = -1)
			{
				fd = File_descriptor_registry::add_io_channel(io_channel, fd);

				/* Register the interrupt handler only once per IO channel */
				if (_is_the_only_fd_for_io_channel(fd, io_channel)) {
					Io_channel_listener *l = new (env()->heap()) Io_channel_listener(this);
					io_channel->register_interrupt_handler(l);
				}

				return fd;
			}

			void remove_io_channel(int fd)
			{
				Shared_pointer<Io_channel> io_channel = _lookup_channel(fd);

				/*
				 * Unregister the interrupt handler only if there are no other
				 * file descriptors associated with the IO channel.
				 */
				if (_is_the_only_fd_for_io_channel(fd, io_channel)) {
					Io_channel_listener *l = io_channel->lookup_io_channel_listener(this);
					io_channel->unregister_interrupt_handler(l);
					Genode::destroy(env()->heap(), l);
				}

				File_descriptor_registry::remove_io_channel(fd);
			}

			void flush()
			{
				for (int fd = 0; fd < MAX_FILE_DESCRIPTORS; fd++)
					try {
						remove_io_channel(fd);
					} catch (Invalid_fd) { }
			}


			/*****************************
			 ** Family_member interface **
			 *****************************/

			void submit_signal(Noux::Sysio::Signal sig)
			{
				try {
					_pending_signals.add(sig);
				} catch (Signal_queue::Overflow) {
					error("signal queue is full - signal dropped");
				}

				_blocker.unlock();
			}

			Family_member *do_execve(const char *filename,
			                         Args const &args,
			                         Sysio::Env const &env,
			                         bool verbose)
			{
				Lock::Guard signal_lock_guard(signal_lock());

				Child *child = new Child(filename,
				                         _parent_exit,
				                         _kill_broadcaster,
				                         _parent_execve,
				                         pid(),
				                         _sig_rec,
				                         _root_dir,
				                         args,
				                         env,
				                         _env_pd_session,
				                         _ref_ram, _ref_ram_cap,
				                         _parent_services,
				                         _ep,
				                         false,
				                         *Genode::env()->heap(),
				                         _destruct_queue,
				                         verbose);

				_assign_io_channels_to(child);

				/* move the signal queue */
				while (!_pending_signals.empty())
					child->_pending_signals.add(_pending_signals.get());

				/*
				 * Close all open files.
				 *
				 * This action is not part of the child destructor,
				 * because in the case that a child exits itself,
				 * it may need to close all files to unblock the
				 * parent (which might be reading from a pipe) before
				 * the parent can destroy the child object.
				 */
				flush();

				/* signal main thread to remove ourself */
				Genode::Signal_transmitter(_destruct_context_cap).submit();

				/* start executing the new process */
				child->start();

				/* this child will be removed by the execve_finalization_dispatcher */

				return child;
			}

			/*********************************
			 ** Interrupt_handler interface **
			 *********************************/

			void handle_interrupt()
			{
				submit_signal(Sysio::SIG_INT);
			}

	};
};

#endif /* _NOUX__CHILD_H_ */
