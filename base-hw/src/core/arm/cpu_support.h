/*
 * \brief   CPU specific support for base-hw
 * \author  Martin Stein
 * \date    2013-11-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread_event.h>

namespace Kernel
{
	/**
	 * CPU specific parts of a kernel thread-object
	 */
	class Thread_cpu_support;
}

class Kernel::Thread_cpu_support
{
	protected:

		Thread_event _fault;
		addr_t       _fault_tlb;
		addr_t       _fault_addr;
		addr_t       _fault_writes;
		addr_t       _fault_signal;

		/**
		 * Constructor
		 *
		 * \param t  generic part of kernel thread-object
		 */
		Thread_cpu_support(Thread * const t);
};
