/*
 * \brief  Interfaces for initializing libc subsystems
 * \author Norman Feske
 * \date   2016-10-27
 */

/*
 * Copyright (C) 2016-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__INIT_H_
#define _LIBC__INTERNAL__INIT_H_

/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <base/node.h>
#include <util/callable.h>
#include <vfs/types.h>  /* for 'MAX_PATH_LEN' */

/* libc includes */
#include <setjmp.h>     /* for 'jmp_buf' type */
#include <libc/component.h>

/* libc-internal includes */
#include <internal/types.h>
#include <internal/config.h>

namespace Libc {

	struct Resume;
	struct Suspend;
	struct Monitor;
	struct Select;
	struct Current_time;
	struct Current_real_time;
	struct Clone_connection;
	struct Watch;
	struct Signal;
	struct File_descriptor_allocator;
	struct Timer_accessor;
	struct Cwd;
	struct Atexit;

	/**
	 * Support for shared libraries
	 */
	void init_dl(Genode::Env &env);

	/**
	 * File-descriptor allocator
	 */
	void init_fd_alloc(Genode::Allocator &);

	/**
	 * Global memory allocator
	 */
	void init_mem_alloc(Genode::Env &env);

	/**
	 * Plugin interface
	 */
	void init_plugin(Resume &);

	/**
	 * Virtual file system
	 */
	void init_vfs_plugin(Monitor &, Genode::Env::Local_rm &);
	void init_file_operations(Cwd &, File_descriptor_allocator &, Config_accessor const &);
	void init_pread_pwrite(File_descriptor_allocator &);

	/**
	 * Poll support
	 */
	void init_poll(Signal &, Monitor &, File_descriptor_allocator &);

	/**
	 * Select support
	 */
	void init_select(Select &);

	/**
	 * Support for querying available RAM quota in sysctl functions
	 */
	void sysctl_init(Genode::Env &env);

	/**
	 * Support for getpwent
	 */
	void init_passwd(Node const &);

	/**
	 * Malloc allocator
	 */
	void init_malloc(Genode::Allocator &);
	void init_malloc_cloned(Clone_connection &);
	void reinit_malloc(Genode::Allocator &);

	using Rtc_path = String<Vfs::MAX_PATH_LEN>;

	/**
	 * Init timing facilities
	 */
	void init_sleep(Monitor &);
	void init_time(Current_time &, Current_real_time &);
	void init_alarm(Timer_accessor &, Signal &);

	/**
	 * Socket fs
	 */
	void init_socket_fs(Monitor &, File_descriptor_allocator &, Config const &);
	void init_socket_operations(File_descriptor_allocator &, Config const &);

	/**
	 * Pthread/semaphore support
	 */
	void init_pthread_support(Monitor &, Timer_accessor &);
	void init_pthread_support(Genode::Cpu_session &, Node const &,
	                          Genode::Allocator &);
	void init_semaphore_support(Timer_accessor &);

	/**
	 * Fork mechanism
	 */
	void init_fork(Genode::Env &, File_descriptor_allocator &,
	               Config_accessor const &, Genode::Allocator &heap,
	               Heap &malloc_heap, int pid, Monitor &, Signal &,
	               Binary_name const &);

	struct Reset_malloc_heap : Interface
	{
		virtual void reset_malloc_heap() = 0;
	};

	/**
	 * Execve mechanism
	 */
	void init_execve(Genode::Env &, Genode::Allocator &, void *user_stack,
	                 Reset_malloc_heap &, Binary_name &,
	                 File_descriptor_allocator &);

	/**
	 * Signal handling
	 */
	void init_signal(Signal &);

	/**
	 * Atexit handling
	 */
	void init_atexit(Atexit &);

	/**
	 * Kqueue support
	 */
	void init_kqueue(Genode::Allocator &, Monitor &, File_descriptor_allocator &);

	/**
	 * Random-number support
	 */
	void init_random(Config const &);
}

#endif /* _LIBC__INTERNAL__INIT_H_ */
