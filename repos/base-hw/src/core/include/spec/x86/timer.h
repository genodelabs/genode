/*
 * \brief  Timer driver for core
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <base/stdint.h>
#include <base/printf.h>


namespace Genode
{
	/**
	 * Timer driver for core
	 *
	 * Timer channel 0 apparently doesn't work on the RPI, so we use channel 1
	 */
	class Timer;
}

class Genode::Timer
{
	public:

		Timer() { }

		static unsigned interrupt_id(int)
		{
			PDBG("not implemented");
			return 0;
		}

		inline void start_one_shot(uint32_t const tics, unsigned)
		{
			PDBG("not implemented");
		}

		static uint32_t ms_to_tics(unsigned const ms)
		{
			PDBG("not implemented");
			return 10000;
		}

		unsigned value(unsigned)
		{
			PDBG("not implemented");
			return 0;
		}
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
