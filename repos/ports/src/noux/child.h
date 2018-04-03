/*
 * \brief  Noux child process
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__CHILD_H_
#define _NOUX__CHILD_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <vfs/dir_file_system.h>

/* Noux includes */
#include <file_descriptor_registry.h>
#include <noux_session/capability.h>
#include <args.h>
#include <environment.h>
#include <cpu_session_component.h>
#include <pd_session_component.h>
#include <child_policy.h>
#include <io_receptor_registry.h>
#include <destruct_queue.h>
#include <interrupt_handler.h>
#include <kill_broadcaster.h>
#include <parent_execve.h>
#include <empty_rom_service.h>
#include <local_rom_service.h>
#include <verbose.h>
#include <user_info.h>
#include <timeout_scheduler.h>

namespace Noux {

	class Pid_allocator;

	/**
	 * Return singleton instance of Io_receptor_registry
	 */
	Io_receptor_registry *io_receptor_registry();

	/*
	 * Return lock for protecting the signal queue
	 */
	Genode::Lock &signal_lock();

	class Child_config;
	class Child;

	/**
	 * Return true is child is the init process
	 */
	bool init_process(Child *child);
	void init_process_exited(int);
}


/**
 * Allocator for process IDs
 */
class Noux::Pid_allocator
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


struct Noux::Child_config : Attached_ram_dataspace
{
	enum { CONFIG_DS_SIZE = 4096 };

	Child_config(Ram_allocator &ram, Region_map &local_rm, Verbose const &verbose)
	:
		Attached_ram_dataspace(ram, local_rm, CONFIG_DS_SIZE)
	{
		Xml_generator xml(local_addr<char>(), CONFIG_DS_SIZE, "config", [&] ()
		{
			if (verbose.ld())
				xml.attribute("ld_verbose", "yes");
		});
	}
};


class Noux::Child : public Rpc_object<Session>,
                    public File_descriptor_registry,
                    public Family_member,
                    public Destruct_queue::Element<Child>,
                    public Interrupt_handler
{
	private:

		Child_policy::Name const _name;

		Verbose const &_verbose;

		User_info const &_user_info;

		Parent_exit       *_parent_exit;
		Kill_broadcaster  &_kill_broadcaster;
		Timeout_scheduler &_timeout_scheduler;
		Parent_execve     &_parent_execve;
		Pid_allocator     &_pid_allocator;

		Env &_env;

		Vfs::File_system &_root_dir;

		Vfs_io_waiter_registry &_vfs_io_waiter_registry;
		Vfs_handle_context      _vfs_handle_context;

		Destruct_queue &_destruct_queue;

		void _handle_destruct() { _destruct_queue.insert(this); }

		Signal_handler<Child> _destruct_handler {
			_env.ep(), *this, &Child::_handle_destruct };

		Allocator &_heap;

		/**
		 * Entrypoint used to serve the RPC interfaces of the
		 * locally-provided services
		 */
		enum { STACK_SIZE = 8*1024*sizeof(long) };
		Rpc_entrypoint _ep { &_env.pd(), STACK_SIZE, "noux_process", false };

		Pd_session                  &_ref_pd;
		Pd_session_capability  const _ref_pd_cap;

		/**
		 * Registry of dataspaces owned by the Noux process
		 */
		Dataspace_registry _ds_registry { _heap };

		/**
		 * Locally-provided PD service
		 */
		typedef Local_service<Pd_session_component> Pd_service;
		Pd_session_component _pd { _heap, _env, _ep, _name, _ds_registry };
		Pd_service::Single_session_factory _pd_factory { _pd };
		Pd_service                         _pd_service { _pd_factory };

		/**
		 * Locally-provided CPU service
		 */
		typedef Local_service<Cpu_session_component> Cpu_service;
		Cpu_session_component _cpu { _env, _ep, _name, false, _ds_registry };
		Cpu_service::Single_session_factory _cpu_factory { _cpu };
		Cpu_service                         _cpu_service { _cpu_factory };

		/*
		 * Locally-provided Noux service
		 */
		Capability_guard _cap_guard { _ep, *this };

		typedef Local_service<Rpc_object<Session> > Noux_service;
		Noux_service::Single_session_factory _noux_factory { *this };
		Noux_service                         _noux_service { _noux_factory };

		/*
		 * Locally-provided ROM service
		 */
		Empty_rom_factory _empty_rom_factory { _heap, _ep };
		Empty_rom_service _empty_rom_service { _empty_rom_factory };
		Local_rom_factory _rom_factory { _heap, _env, _ep, _root_dir,
		                                 _vfs_io_waiter_registry, _ds_registry };
		Local_rom_service _rom_service { _rom_factory };

		/**
		 * Command line arguments
		 */
		Args_dataspace _args;

		/**
		 * Environment variables
		 */
		Environment _sysio_env;

		/*
		 * Child configuration
		 */
		Child_config _config { _ref_pd, _env.rm(), _verbose };

		enum { PAGE_SIZE = 4096, PAGE_MASK = ~(PAGE_SIZE - 1) };
		enum { SYSIO_DS_SIZE = PAGE_MASK & (sizeof(Sysio) + PAGE_SIZE - 1) };

		Attached_ram_dataspace _sysio_ds { _ref_pd, _env.rm(), SYSIO_DS_SIZE };
		Sysio &_sysio = *_sysio_ds.local_addr<Sysio>();

		typedef Ring_buffer<enum Sysio::Signal, Sysio::SIGNAL_QUEUE_SIZE>
		        Signal_queue;
		Signal_queue _pending_signals;

		Parent_services &_parent_services;

		Static_dataspace_info _sysio_ds_info;
		Static_dataspace_info _args_ds_info;
		Static_dataspace_info _sysio_env_ds_info;
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
		void _assign_io_channels_to(Child *child, bool skip_when_close_on_execve_set)
		{
			for (int fd = 0; fd < MAX_FILE_DESCRIPTORS; fd++)
				if (fd_in_use(fd)) {
					if (skip_when_close_on_execve_set && close_fd_on_execve(fd))
						continue;
					child->add_io_channel(io_channel_by_fd(fd), fd);
					child->close_fd_on_execve(fd, close_fd_on_execve(fd));
				}
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

		void _destruct()
		{
			_ep.dissolve(this);

			if (init_process(this))
				init_process_exited(_child_policy.exit_value());
		}

	public:

		struct Insufficient_memory : Exception { };

		/**
		 * Constructor
		 *
		 * \param forked  false if the child is spawned directly from
		 *                an executable binary (i.e., the init process,
		 *                or children created via execve, or
		 *                true if the child is a fork from another child
		 *
		 * \throw Insufficent_memory if the child could not be started by
		 *                           the parent
		 */
		Child(Child_policy::Name const &name,
		      Verbose            const &verbose,
		      User_info          const &user_info,
		      Parent_exit              *parent_exit,
		      Kill_broadcaster         &kill_broadcaster,
		      Timeout_scheduler        &timeout_scheduler,
		      Parent_execve            &parent_execve,
		      Pid_allocator            &pid_allocator,
		      int                       pid,
		      Env                      &env,
		      Vfs::File_system         &root_dir,
		      Vfs_io_waiter_registry   &vfs_io_waiter_registry,
		      Args               const &args,
		      Sysio::Env         const &sysio_env,
		      Allocator                &heap,
		      Pd_session               &ref_pd,
		      Pd_session_capability     ref_pd_cap,
		      Parent_services          &parent_services,
		      bool                      forked,
		      Destruct_queue           &destruct_queue)
		:
			Family_member(pid),
			Destruct_queue::Element<Child>(heap),
			_name(name),
			_verbose(verbose),
			_user_info(user_info),
			_parent_exit(parent_exit),
			_kill_broadcaster(kill_broadcaster),
			_timeout_scheduler(timeout_scheduler),
			_parent_execve(parent_execve),
			_pid_allocator(pid_allocator),
			_env(env),
			_root_dir(root_dir),
			_vfs_io_waiter_registry(vfs_io_waiter_registry),
			_destruct_queue(destruct_queue),
			_heap(heap),
			_ref_pd (ref_pd), _ref_pd_cap (ref_pd_cap),
			_args(ref_pd, _env.rm(), ARGS_DS_SIZE, args),
			_sysio_env(_ref_pd, _env.rm(), sysio_env),
			_parent_services(parent_services),
			_sysio_ds_info(_ds_registry, _sysio_ds.cap()),
			_args_ds_info(_ds_registry, _args.cap()),
			_sysio_env_ds_info(_ds_registry, _sysio_env.cap()),
			_config_ds_info(_ds_registry, _config.cap()),
			_child_policy(name, forked,
			              _args.cap(), _sysio_env.cap(), _config.cap(),
			              _ep, _pd_service, _cpu_service,
			              _noux_service, _empty_rom_service,
			              _rom_service, _parent_services,
			              *this, parent_exit, *this, _destruct_handler,
			              ref_pd, ref_pd_cap,
			              _verbose.enabled()),
			_child(_env.rm(), _ep, _child_policy)
		{
			if (_verbose.enabled())
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

			_pd.poke(_env.rm(), parent_cap_addr, (char *)&raw, sizeof(raw));

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
				Signal_transmitter(_destruct_handler).submit();
			}
		}

		Pd_session_component &pd() { return _pd;  }

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
				Io_channel_listener *l = new (_heap) Io_channel_listener(this);
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
				Genode::destroy(_heap, l);
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
		                         Sysio::Env const &env) override
		{
			Lock::Guard signal_lock_guard(signal_lock());

			Child *child = new (_heap) Child(filename,
			                                 _verbose,
			                                 _user_info,
			                                 _parent_exit,
			                                 _kill_broadcaster,
			                                 _timeout_scheduler,
			                                 _parent_execve,
			                                 _pid_allocator,
			                                 pid(),
			                                 _env,
			                                 _root_dir,
			                                 _vfs_io_waiter_registry,
			                                 args,
			                                 env,
			                                 _heap,
			                                 _ref_pd, _ref_pd_cap,
			                                 _parent_services,
			                                 false,
			                                 _destruct_queue);

			_assign_io_channels_to(child, true);

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
			Signal_transmitter(_destruct_handler).submit();

			/* start executing the new process */
			child->start();

			/* this child will be removed by the execve_finalization_dispatcher */

			return child;
		}


		/*********************************
		 ** Interrupt_handler interface **
		 *********************************/

		void handle_interrupt(Sysio::Signal signal)
		{
			submit_signal(signal);
		}
};

#endif /* _NOUX__CHILD_H_ */
