/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SPEC__PBXA9__BOARD_H_
#define _SPEC__PBXA9__BOARD_H_

/* core includes */
#include <spec/cortex_a9/board_support.h>

namespace Genode
{
	class Board : public Cortex_a9::Board
	{
		public:

			static void outer_cache_invalidate() { }
			static void outer_cache_flush() { }
			static void prepare_kernel() { }
			static void secondary_cpus_ip(void * const ip) { }
			static bool is_smp() { return false; }
	};
}

#endif /* _SPEC__PBXA9__BOARD_H_ */
