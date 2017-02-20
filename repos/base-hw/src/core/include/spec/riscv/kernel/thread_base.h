/*
 * \brief   Hardware specific base of kernel thread-objects
 * \author  Sebastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KERNEL__THREAD_BASE_H_
#define _KERNEL__THREAD_BASE_H_

/* core includes */
#include <kernel/thread_event.h>

namespace Kernel { class Thread; }

/**
 * Hardware specific base of kernel thread-objects
 */
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
