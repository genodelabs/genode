/*
 * \brief  Board driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* core includes */
#include <spec/cortex_a15/board_support.h>

namespace Genode
{
	class Board : public Cortex_a15::Board_base
	{
		public:

			static void outer_cache_invalidate() { }
			static void outer_cache_flush() { }
			static void prepare_kernel();

			/**
			 * Tell secondary CPUs to start execution from instr. pointer 'ip'
			 */
			static void secondary_cpus_ip(void * const ip) {
				*(void * volatile *)IRAM_BASE = ip; }

			static bool is_smp() { return true; }
	};
}

#endif /* _BOARD_H_ */
