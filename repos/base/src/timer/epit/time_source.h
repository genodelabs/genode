/*
 * \brief  Time source that uses the Enhanced Periodic Interrupt Timer (Freescale)
 * \author Norman Feske
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
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

class Timer::Time_source : private Genode::Attached_mmio<0x14>,
                           public Genode::Signalled_time_source
{
	private:

		enum { TICKS_PER_MS = 66000 };

		struct Cr : Register<0x0, 32>
		{
			struct En      : Bitfield<0, 1>  { };
			struct En_mod  : Bitfield<1, 1>  { enum { RELOAD = 1 }; };
			struct Oci_en  : Bitfield<2, 1>  { };
			struct Swr     : Bitfield<16, 1> { };
			struct Clk_src : Bitfield<24, 2> { enum { HIGH_FREQ_REF_CLK = 2 }; };

			static access_t prepare_one_shot()
			{
				access_t cr = 0;
				En_mod::set(cr, En_mod::RELOAD);
				Oci_en::set(cr, 1);
				Clk_src::set(cr, Clk_src::HIGH_FREQ_REF_CLK);
				return cr;
			}
		};

		struct Sr   : Register<0x4,  32> { struct Ocif : Bitfield<0, 1> { }; };
		struct Cmpr : Register<0xc,  32> { };
		struct Cnt  : Register<0x10, 32> { enum { MAX = ~(access_t)0 }; };

		Genode::Irq_connection     _timer_irq;
		Genode::Duration           _curr_time     { Genode::Microseconds(0) };
		Genode::Microseconds const _max_timeout   { Genode::timer_ticks_to_us(Cnt::MAX / 2, TICKS_PER_MS) };
		unsigned long              _cleared_ticks { 0 };

	public:

		Time_source(Genode::Env &env);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Genode::Duration curr_time() override;
		void set_timeout(Genode::Microseconds, Genode::Timeout_handler &) override;
		Genode::Microseconds max_timeout() const override { return _max_timeout; };
};

#endif /* _TIME_SOURCE_H_ */
