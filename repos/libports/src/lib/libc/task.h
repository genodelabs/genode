/*
 * \brief  Libc-internal kernel API
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2016-12-14
 *
 * TODO document libc tasking including
 * - the initial thread (which is neither component nor pthread)
 *   - processes incoming signals and forwards to entrypoint
 * - the main thread (which is the kernel and the main user context)
 * - pthreads (which are pthread user contexts only)
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__TASK_H_
#define _LIBC__TASK_H_

#include <base/duration.h>
#include <util/xml_node.h>
#include <vfs/vfs_handle.h>

namespace Libc {

	/**
	 * Resume all user contexts
	 *
	 * This resumes the main user context as well as any pthread context.
	 */
	void resume_all();

	/**
	 * Suspend the execution of the calling user context
	 *
	 * \param timeout_ms  maximum time to stay suspended in milliseconds,
	 *                    0 for infinite suspend
	 *
	 * \return            remaining duration until timeout,
	 *                    0 if the timeout expired
	 *
	 * The context could be running on the component entrypoint as main context
	 * or as separate pthread. This function returns after the libc kernel
	 * resumed the user context execution.
	 */
	struct Suspend_functor { virtual bool suspend() = 0; };
	Genode::uint64_t suspend(Suspend_functor &, Genode::uint64_t timeout_ms = 0);

	void dispatch_pending_io_signals();

	/**
	 * Get watch handle for given path
	 *
	 * \param path  path that should be be watched
	 *
	 * \return      point to the watch handle object or a nullptr
	 *              when the watch operation failed
	 */
	Vfs::Vfs_watch_handle *watch(char const *path);

	/**
	 * Get time since startup in ms
	 */
	Genode::Duration current_time();

	/**
	 * Suspend main user context and the component entrypoint
	 *
	 * This interface is solely used by the implementation of fork().
	 */
	void schedule_suspend(void (*suspended) ());

	struct Select_handler_base;

	/**
	 * Schedule select handler that is deblocked by ready fd sets
	 */
	void schedule_select(Select_handler_base *);

	struct Kernel_routine : Genode::Interface
	{
		virtual void execute_in_kernel() = 0;
	};

	/**
	 * Register routine to be called once on the next libc-kernel activation
	 *
	 * The specified routine is executed only once. For a repeated execution,
	 * the routine must call 'register_kernel_routine' with itself as
	 * argument.
	 *
	 * This mechanism is used by 'fork' to implement the blocking for the
	 * startup of a new child and for 'wait4'.
	 */
	void register_kernel_routine(Kernel_routine &);

	/**
	 * Access libc configuration Xml_node.
	 */
	Genode::Xml_node libc_config();
}

#endif /* _LIBC__TASK_H_ */
