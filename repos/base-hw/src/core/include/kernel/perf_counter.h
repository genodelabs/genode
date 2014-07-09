/*
 * \brief   Performance counter specific functions
 * \author  Josef Soentgen
 * \date    2013-09-26
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__PERF_COUNTER_H_
#define _KERNEL__PERF_COUNTER_H_

namespace Kernel
{
	/**
	 * Performance counter
	 */
	class Perf_counter
	{
		public:

			/**
			 * Enable counting
			 */
			void enable();
	};


	extern Perf_counter * perf_counter();
}

#endif /* _KERNEL__PERF_COUNTER_H_ */
