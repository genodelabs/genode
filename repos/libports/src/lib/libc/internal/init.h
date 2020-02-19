/*
 * \brief  Interfaces for initializing libc subsystems
 * \author Norman Feske
 * \date   2016-10-27
 */

/*
 * Copyright (C) 2016-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__INIT_H_
#define _LIBC__INTERNAL__INIT_H_

/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <util/xml_node.h>
#include <vfs/types.h>  /* for 'MAX_PATH_LEN' */

/* libc includes */
#include <setjmp.h>     /* for 'jmp_buf' type */

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	struct Resume;
	struct Suspend;
	struct Monitor;
	struct Select;
	struct Current_time;
	struct Clone_connection;
	struct Kernel_routine_scheduler;
	struct Watch;
	struct Signal;
	struct File_descriptor_allocator;
	struct Timer_accessor;

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
	void init_vfs_plugin(Suspend &);

	/**
	 * Select support
	 */
	void init_select(Suspend &, Resume &, Select &, Signal &);

	/**
	 * Support for querying available RAM quota in sysctl functions
	 */
	void sysctl_init(Genode::Env &env);

	/**
	 * Support for getpwent
	 */
	void init_passwd(Xml_node);

	/**
	 * Set libc config node
	 */
	void libc_config_init(Xml_node node);

	/**
	 * Malloc allocator
	 */
	void init_malloc(Genode::Allocator &);
	void init_malloc_cloned(Clone_connection &);
	void reinit_malloc(Genode::Allocator &);

	typedef String<Vfs::MAX_PATH_LEN> Rtc_path;

	/**
	 * Init timing facilities
	 */
	void init_sleep(Suspend &);
	void init_time(Current_time &, Rtc_path const &, Watch &);

	/**
	 * Socket fs
	 */
	void init_socket_fs(Suspend &);

	/**
	 * Pthread/semaphore support
	 */
	void init_pthread_support(Suspend &, Resume &, Timer_accessor &);
	void init_semaphore_support(Timer_accessor &);

	struct Config_accessor : Interface
	{
		virtual Xml_node config() const = 0;
	};

	/**
	 * Fork mechanism
	 */
	void init_fork(Genode::Env &, Config_accessor const &,
	               Genode::Allocator &heap, Heap &malloc_heap, int pid,
	               Suspend &, Resume &, Signal &, Kernel_routine_scheduler &,
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
}

#endif /* _LIBC__INTERNAL__INIT_H_ */
