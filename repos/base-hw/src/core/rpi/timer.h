/*
 * \brief  Timer for kernel
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _RPI__TIMER_H_
#define _RPI__TIMER_H_

/* Genode includes */
#include <util/mmio.h>
#include <base/stdint.h>
#include <base/printf.h>
#include <drivers/board_base.h>

namespace Kernel { class Timer; }


class Kernel::Timer : public Genode::Mmio
{
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

	private:

		typedef Genode::uint32_t   uint32_t;
		typedef Genode::Board_base Board_base;

	public:

		Timer() : Mmio(Board_base::SYSTEM_TIMER_MMIO_BASE) { }

		static unsigned interrupt_id(unsigned)
		{
			return Board_base::SYSTEM_TIMER_IRQ;
		}

		inline void start_one_shot(uint32_t const tics, unsigned)
		{
			write<Clo>(0);
			write<Cmp>(read<Clo>() + tics);
			write<Cs::Status>(1);
		}

		static uint32_t ms_to_tics(unsigned const ms)
		{
			return (Board_base::SYSTEM_TIMER_CLOCK / 1000) * ms;
		}

		void clear_interrupt(unsigned)
		{
			write<Cs::Status>(1);
			read<Cs>();
		}
};

#endif /* _RPI__TIMER_H_ */

