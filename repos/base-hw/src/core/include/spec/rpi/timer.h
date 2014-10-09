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

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Genode
{
	/**
	 * Timer driver for core
	 *
	 * Timer channel 0 apparently doesn't work on the RPI, so we use channel 1
	 */
	class Timer;
}

class Genode::Timer : public Mmio
{
	private:

		struct Cs  : Register<0x0, 32> { struct M1 : Bitfield<1, 1> { }; };
		struct Clo : Register<0x4,  32> { };
		struct Cmp : Register<0x10, 32> { };

	public:

		Timer() : Mmio(Board::SYSTEM_TIMER_MMIO_BASE) { }

		static unsigned interrupt_id(int) { return Board::SYSTEM_TIMER_IRQ; }

		inline void start_one_shot(uint32_t const tics, unsigned)
		{
			write<Cs::M1>(1);
			read<Cs>();
			write<Clo>(0);
			write<Cmp>(read<Clo>() + tics);
		}

		static uint32_t ms_to_tics(unsigned const ms) {
			return (Board::SYSTEM_TIMER_CLOCK / 1000) * ms; }

		unsigned value(unsigned)
		{
			Cmp::access_t const cmp = read<Cmp>();
			Clo::access_t const clo = read<Clo>();
			return cmp > clo ? cmp - clo : 0;
		}
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
