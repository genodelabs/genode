/*
 * \brief  Board driver for core on Zynq
 * \author Johannes Schlatow
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* core includes */
#include <spec/cortex_a9/board_support.h>
#include <spec/arm/pl310.h>

namespace Genode
{
	class Pl310;
	class Board;
}

/**
 * L2 outer cache controller
 */
class Genode::Pl310 : public Arm::Pl310
{
	public:

		Pl310(addr_t const base) : Arm::Pl310(base) { _init(); }
};

/**
 * Board driver for core
 */
class Genode::Board : public Cortex_a9::Board
{
	public:

		static void outer_cache_invalidate();
		static void outer_cache_flush();
		static void prepare_kernel();
		static void secondary_cpus_ip(void * const ip) { }
		static bool is_smp() { return true; }
};

#endif /* _BOARD_H_ */
