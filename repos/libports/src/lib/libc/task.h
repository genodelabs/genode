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

/* Genode includes */
#include <os/timeout.h>


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
	unsigned long suspend(Suspend_functor &, unsigned long timeout_ms = 0UL);

	/**
	 * Get time since startup in ms
	 */
	unsigned long current_time();

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
}

#endif /* _LIBC__TASK_H_ */
