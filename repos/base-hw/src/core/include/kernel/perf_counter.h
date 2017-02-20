/*
 * \brief   Performance counter specific functions
 * \author  Josef Soentgen
 * \date    2013-09-26
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__KERNEL__PERF_COUNTER_H_
#define _CORE__INCLUDE__KERNEL__PERF_COUNTER_H_

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

#endif /* _CORE__INCLUDE__KERNEL__PERF_COUNTER_H_ */
