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
	 */
	class Timer;
}

class Genode::Timer : public Mmio
{
	private:

		/*
		 * The timer channel 0 apparently does not work on the Raspberry Pi.
		 * So we use channel 1.
		 */

		struct Cs : Register<0x0, 32>
		{
			struct Status : Bitfield<1, 1> { };
		};

		struct Clo : Register<0x4,  32> { };
		struct Cmp : Register<0x10, 32> { };

	public:

		Timer() : Mmio(Board::SYSTEM_TIMER_MMIO_BASE) { }

		static unsigned interrupt_id(unsigned)
		{
			return Board::SYSTEM_TIMER_IRQ;
		}

		inline void start_one_shot(uint32_t const tics, unsigned)
		{
			write<Clo>(0);
			write<Cmp>(read<Clo>() + tics);
			write<Cs::Status>(1);
		}

		static uint32_t ms_to_tics(unsigned const ms)
		{
			return (Board::SYSTEM_TIMER_CLOCK / 1000) * ms;
		}

		void clear_interrupt(unsigned)
		{
			write<Cs::Status>(1);
			read<Cs>();
		}
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
