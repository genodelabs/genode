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

#ifndef _PERF_COUNTER_H_
#define _PERF_COUNTER_H_

namespace Kernel {

	class Perf_counter
	{
		public:

			void enable();
	};


	extern Perf_counter *perf_counter();

}

#endif /* _PERF_COUNTER_H_ */
