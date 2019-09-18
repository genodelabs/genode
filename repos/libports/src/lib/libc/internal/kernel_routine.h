/*
 * \brief  Interface executing code in the context of the libc kernel
 * \author Norman Feske
 * \date   2019-09-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__KERNEL_ROUTINE_H_
#define _LIBC__INTERNAL__KERNEL_ROUTINE_H_

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	/**
	 * Base class to be implemented by a kernel routine
	 */
	struct Kernel_routine : Interface
	{
		virtual void execute_in_kernel() = 0;
	};

	struct Kernel_routine_scheduler : Interface
	{
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
		virtual void register_kernel_routine(Kernel_routine &) = 0;
	};
}

#endif /* _LIBC__INTERNAL__KERNEL_ROUTINE_H_ */
