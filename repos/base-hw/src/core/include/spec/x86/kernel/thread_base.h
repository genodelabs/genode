/*
 * \brief   Hardware specific base of kernel thread-objects
 * \author  Martin Stein
 * \date    2013-11-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__THREAD_BASE_H_
#define _KERNEL__THREAD_BASE_H_

/* core includes */
#include <kernel/thread_event.h>

namespace Kernel
{
	/**
	 * Hardware specific base of kernel thread-objects
	 */
	class Thread_base;
}

class Kernel::Thread_base
{
	protected:

		Thread_event _fault;
		addr_t       _fault_pd;
		addr_t       _fault_addr;
		addr_t       _fault_writes;
		addr_t       _fault_signal;

		/**
		 * Constructor
		 *
		 * \param t  generic part of kernel thread-object
		 */
		Thread_base(Thread * const t);
};

#endif /* _KERNEL__THREAD_BASE_H_ */
