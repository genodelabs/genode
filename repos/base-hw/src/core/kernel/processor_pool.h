/*
 * \brief   Provide a processor object for every available processor
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__PROCESSOR_POOL_H_
#define _KERNEL__PROCESSOR_POOL_H_

/* base includes */
#include <unmanaged_singleton.h>

/* core includes */
#include <kernel/kernel.h>
#include <kernel/thread.h>

namespace Kernel
{
	/**
	 * Thread that consumes processor time if no other thread is available
	 */
	class Idle_thread;

	/**
	 * Provides a processor object for every available processor
	 */
	class Processor_pool;

	/**
	 * Return Processor_pool singleton
	 */
	Processor_pool * processor_pool();
}

class Kernel::Idle_thread : public Thread
{
	private:

		enum {
			STACK_SIZE   = sizeof(addr_t) * 32,
			STACK_ALIGNM = Processor_driver::DATA_ACCESS_ALIGNM,
		};

		char _stack[STACK_SIZE] __attribute__((aligned(STACK_ALIGNM)));

		/**
		 * Main function of all idle threads
		 */
		static void _main()
		{
			while (1) { Processor_driver::wait_for_interrupt(); }
		}

	public:

		/**
		 * Constructor
		 *
		 * \param processor  kernel object of targeted processor
		 */
		Idle_thread(Processor * const processor)
		:
			Thread(Priority::MAX, "idle")
		{
			ip = (addr_t)&_main;
			sp = (addr_t)&_stack[STACK_SIZE];
			init(processor, core_pd(), 0, 0);
		}
};

class Kernel::Processor_pool
{
	private:

		char _processors[PROCESSORS][sizeof(Processor)];
		char _idle_threads[PROCESSORS][sizeof(Idle_thread)];

		/**
		 * Return idle thread of a specific processor
		 *
		 * \param processor_id  kernel name of the targeted processor
		 */
		Idle_thread * _idle_thread(unsigned const processor_id) const
		{
			char * const p = const_cast<char *>(_idle_threads[processor_id]);
			return reinterpret_cast<Idle_thread *>(p);
		}

	public:

		/**
		 * Constructor
		 */
		Processor_pool()
		{
			for (unsigned i = 0; i < PROCESSORS; i++) {
				new (_idle_threads[i]) Idle_thread(processor(i));
				new (_processors[i]) Processor(i, _idle_thread(i));
			}
		}

		/**
		 * Return the object of a specific processor
		 *
		 * \param id  kernel name of the targeted processor
		 */
		Processor * processor(unsigned const id) const
		{
			assert(id < PROCESSORS);
			char * const p = const_cast<char *>(_processors[id]);
			return reinterpret_cast<Processor *>(p);
		}

		/**
		 * Return the object of the primary processor
		 */
		Processor * primary_processor() const
		{
			return processor(Processor::primary_id());
		}
};

#endif /* _KERNEL__PROCESSOR_POOL_H_ */
