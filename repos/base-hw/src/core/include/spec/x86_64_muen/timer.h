/*
 * \brief  Timer driver for core
 * \author Reto Buerki
 * \date   2015-04-14
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <base/stdint.h>

namespace Genode
{
	/**
	 * Timer driver for core on Muen
	 */
	class Timer;
}

class Genode::Timer
{
	public:

		Timer() { }

		static unsigned interrupt_id(int) { return 0; }

		inline void start_one_shot(uint32_t const tics, unsigned) { }

		uint32_t ms_to_tics(unsigned const ms) { return 0; }

		unsigned value(unsigned) { return 0; }

		static void disable_pit(void) { }
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
