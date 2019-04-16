/*
 * \brief  Time source that uses the General Purpose Timer (Freescale)
 * \author Stefan Kalkowski
 * \date   2019-04-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIME_SOURCE_H_
#define _TIME_SOURCE_H_

/* Genode includes */
#include <irq_session/connection.h>
#include <os/attached_mmio.h>
#include <drivers/timer/util.h>

/* local includes */
#include <signalled_time_source.h>

namespace Timer { class Time_source; }


class Timer::Time_source : private Genode::Attached_mmio,
                           public  Genode::Signalled_time_source
{
	private:

		enum { TICKS_PER_MS = 500 };

		struct Cr : Register<0x0, 32>
		{
			struct En      : Bitfield<0,  1> { };
			struct En_mod  : Bitfield<1,  1> { };
			struct Clk_src : Bitfield<6,  3> { enum { HIGH_FREQ_REF_CLK = 2 }; };
			struct Frr     : Bitfield<9,  1> { };
			struct Swr     : Bitfield<15, 1> { };
		};

		struct Pr   : Register<0x4,  32> { };
		struct Sr   : Register<0x8,  32> { };
		struct Ir   : Register<0xc,  32> { };
		struct Ocr1 : Register<0x10, 32> { };
		struct Ocr2 : Register<0x14, 32> { };
		struct Ocr3 : Register<0x18, 32> { };
		struct Icr1 : Register<0x1c, 32> { };
		struct Icr2 : Register<0x20, 32> { };
		struct Cnt  : Register<0x24, 32> { };
		

		Genode::Irq_connection     _timer_irq;
		Genode::Duration           _curr_time   { Genode::Microseconds(0) };
		Cnt::access_t              _last_cnt    { 0 };

		void _initialize();

	public:

		Time_source(Genode::Env &env);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Genode::Duration curr_time() override;
		void schedule_timeout(Genode::Microseconds duration,
		                      Timeout_handler &handler) override;
		Genode::Microseconds max_timeout() const override;
};

#endif /* _TIME_SOURCE_H_ */
