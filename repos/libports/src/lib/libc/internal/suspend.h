/*
 * \brief  Interface for suspending the execution until I/O activity
 * \author Norman Feske
 * \date   2019-09-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__SUSPEND_H_
#define _LIBC__INTERNAL__SUSPEND_H_

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	/**
	 * Interface for requesting the condition for suspending
	 */
	struct Suspend_functor : Interface
	{
		virtual bool suspend() = 0;
	};

	struct Suspend : Interface
	{
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
		virtual uint64_t suspend(Suspend_functor &, uint64_t timeout_ms = 0) = 0;
	};
}

#endif /* _LIBC__INTERNAL__SUSPEND_H_ */
