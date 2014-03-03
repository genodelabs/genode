/*
 * \brief   Provide a processor object for every available processor
 * \author  Martin Stein
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__PROCESSOR_H_
#define _KERNEL__PROCESSOR_H_

/* base includes */
#include <unmanaged_singleton.h>

/* core includes */
#include <kernel/thread.h>
#include <processor_driver.h>

namespace Kernel
{
	typedef Cpu_scheduler Processor_scheduler;

	/**
	 * Thread that consumes processor time if no other thread is available
	 */
	class Idle_thread;

	/**
	 * Representation of a single common instruction processor
	 */
	class Processor;

	/**
	 * Provides a processor object for every provided processor
	 */
	class Processor_pool;

	/**
	 * Return Processor_pool singleton
	 */
	Processor_pool * processor_pool();

	/**
	 * Return kernel name of the core protection-domain
	 */
	unsigned core_id();
}


class Kernel::Idle_thread : public Thread
{
	private:

		enum {
			STACK_SIZE   = 4 * 1024,
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
			init(processor, core_id(), 0, 0);
		}
};

class Kernel::Processor : public Processor_driver
{
	private:

		Idle_thread         _idle;
		Processor_scheduler _scheduler;

	public:

		/**
		 * Constructor
		 */
		Processor() : _idle(this), _scheduler(&_idle) { }


		/***************
		 ** Accessors **
		 ***************/

		Processor_scheduler * scheduler() { return &_scheduler; }
};

class Kernel::Processor_pool
{
	private:

		char _data[PROCESSORS][sizeof(Processor)];

	public:

		/**
		 * Initialize the objects of one of the available processors
		 *
		 * \param id  kernel name of the targeted processor
		 */
		Processor_pool()
		{
			for (unsigned i = 0; i < PROCESSORS; i++) {
				new (_data[i]) Processor;
			}
		}

		/**
		 * Return the object of a specific processor
		 *
		 * \param id  kernel name of the targeted processor
		 */
		Processor * select(unsigned const id) const
		{
			return id < PROCESSORS ? (Processor *)_data[id] : 0;
		}

		/**
		 * Return the object of the primary processor
		 */
		Processor * primary() const
		{
			return (Processor *)_data[Processor::primary_id()];
		}
};

#endif /* _KERNEL__PROCESSOR_H_ */
