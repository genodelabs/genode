/*
 * \brief  Timer driver for core
 * \author Martin stein
 * \date   2013-01-10
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Kernel OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER_DRIVER_H_
#define _TIMER_DRIVER_H_

/* Kernel includes */
#include <util/mmio.h>

/* base-hw includes */
#include <kernel/types.h>

namespace Kernel { class Timer_driver; }


struct Kernel::Timer_driver : Genode::Mmio
{
	enum {
		PRESCALER = 1,
		DIV_MUX   = 0,
	};

	/**
	 * MCT configuration
	 */
	struct Mct_cfg : Register<0x0, 32>
	{
		struct Prescaler : Bitfield<0, 8>  { };
		struct Div_mux   : Bitfield<8, 3>  { };
	};


	/*******************
	 ** Local timer 0 **
	 *******************/

	/**
	 * Free running counter buffer
	 */
	struct L0_frcntb : Register<0x310, 32> { };

	/**
	 * Configuration
	 */
	struct L0_tcon : Register<0x320, 32>
	{
		struct Frc_start : Bitfield<3, 1> { };
	};

	/**
	 * Expired status
	 */
	struct L0_int_cstat : Register<0x330, 32, true>
	{
		struct Frcnt : Bitfield<1, 1> { };
	};

	/**
	 * Interrupt enable
	 */
	struct L0_int_enb : Register<0x334, 32>
	{
		struct Frceie : Bitfield<1, 1> { };
	};

	/**
	 * Write status
	 */
	struct L0_wstat : Register<0x340, 32, true>
	{
		struct Frcntb : Bitfield<2, 1> { };
		struct Tcon   : Bitfield<3, 1> { };
	};

	struct L0_frcnto : Register<0x314, 32> { };

	/**
	 * Start and stop counting
	 */
	void run_0(bool const run)
	{
		acked_write<L0_tcon, L0_wstat::Tcon>
			(L0_tcon::Frc_start::bits(run));
	}


	/*******************
	 ** Local timer 1 **
	 *******************/

	/**
	 * Free running counter buffer
	 */
	struct L1_frcntb : Register<0x410, 32> { };

	/**
	 * Configuration
	 */
	struct L1_tcon : Register<0x420, 32>
	{
		struct Frc_start : Bitfield<3, 1> { };
	};

	/**
	 * Expired status
	 */
	struct L1_int_cstat : Register<0x430, 32, true>
	{
		struct Frcnt : Bitfield<1, 1> { };
	};

	/**
	 * Interrupt enable
	 */
	struct L1_int_enb : Register<0x434, 32>
	{
		struct Frceie : Bitfield<1, 1> { };
	};

	/**
	 * Write status
	 */
	struct L1_wstat : Register<0x440, 32, true>
	{
		struct Frcntb : Bitfield<2, 1> { };
		struct Tcon   : Bitfield<3, 1> { };
	};

	struct L1_frcnto : Register<0x414, 32> { };

	/**
	 * Start and stop counting
	 */
	void run_1(bool const run)
	{
		acked_write<L1_tcon, L1_wstat::Tcon>
			(L1_tcon::Frc_start::bits(run));
	}


	/********************
	 ** Helper methods **
	 ********************/

	/**
	 * Write to reg that replies via ack bit and clear ack bit
	 */
	template <typename DEST, typename ACK>
	void acked_write(typename DEST::Register_base::access_t const v)
	{
		typedef typename DEST::Register_base Dest;
		typedef typename ACK::Bitfield_base  Ack;
		write<Dest>(v);
		while (!read<Ack>());
		write<Ack>(1);
	}

	/**
	 * Calculate amount of ticks per ms for specific input clock
	 *
	 * \param clock  input clock
	 */
	time_t static calc_ticks_per_ms(unsigned const clock) {
		return clock / (PRESCALER + 1) / (1 << DIV_MUX) / 1000; }

	unsigned const ticks_per_ms;
	unsigned const cpu_id;

	Timer_driver(unsigned cpu_id);
};

#endif /* _TIMER_DRIVER_H_ */
