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

#ifndef _CORE__INCLUDE__SPEC__RPI__TIMER_H_
#define _CORE__INCLUDE__SPEC__RPI__TIMER_H_

/* base-hw includes */
#include <kernel/types.h>

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Genode { class Timer; }

/**
 * Timer driver for core
 *
 * Timer channel 0 apparently doesn't work on the RPI, so we use channel 1
 */
class Genode::Timer : public Mmio
{
	private:

		using time_t = Kernel::time_t;

		enum { TICS_PER_MS = Board::SYSTEM_TIMER_CLOCK / 1000 };

		struct Cs  : Register<0x0, 32> { struct M1 : Bitfield<1, 1> { }; };
		struct Clo : Register<0x4,  32> { };
		struct Cmp : Register<0x10, 32> { };

	public:

		Timer() : Mmio(Board::SYSTEM_TIMER_MMIO_BASE) { }

		static unsigned interrupt_id(unsigned const) {
			return Board::SYSTEM_TIMER_IRQ; }

		void start_one_shot(time_t const tics, unsigned const)
		{
			write<Cs::M1>(1);
			read<Cs>();
			write<Clo>(0);
			write<Cmp>(read<Clo>() + tics);
		}

		time_t tics_to_us(time_t const tics) const {
			return (tics / TICS_PER_MS) * 1000; }

		time_t us_to_tics(time_t const us) const {
			return (us / 1000) * TICS_PER_MS; }

		time_t max_value() { return (Clo::access_t)~0; }

		time_t value(unsigned const)
		{
			Cmp::access_t const cmp = read<Cmp>();
			Clo::access_t const clo = read<Clo>();
			return cmp > clo ? cmp - clo : 0;
		}
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _CORE__INCLUDE__SPEC__RPI__TIMER_H_ */
