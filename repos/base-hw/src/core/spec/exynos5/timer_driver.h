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


	/*****************
	 ** Local timer **
	 *****************/

	enum Local_timer_offset { L0 = 0x300, L1 = 0x400 };

	struct Local : Genode::Mmio {

		struct Tcntb  : Register<0x0, 32> { };
		struct Tcnto  : Register<0x4, 32> { };
		struct Icntb  : Register<0x8, 32> { };
		struct Icnto  : Register<0xc, 32> { };
		struct Frcntb : Register<0x10, 32> { };
		struct Frcnto : Register<0x14, 32> { };

		struct Tcon : Register<0x20, 32>
		{
			struct Timer_start : Bitfield<0, 1> { };
			struct Irq_start   : Bitfield<1, 1> { };
			struct Irq_type    : Bitfield<2, 1> { };
			struct Frc_start   : Bitfield<3, 1> { };
		};

		struct Int_cstat : Register<0x30, 32, true>
		{
			struct Intcnt : Bitfield<0, 1> { };
			struct Frccnt : Bitfield<1, 1> { };
		};

		struct Int_enb : Register<0x34, 32>
		{
			struct Inteie : Bitfield<0, 1> { };
			struct Frceie : Bitfield<1, 1> { };
		};

		struct Wstat : Register<0x40, 32, true>
		{
			struct Tcntb  : Bitfield<0, 1> { };
			struct Icntb  : Bitfield<1, 1> { };
			struct Frcntb : Bitfield<2, 1> { };
			struct Tcon   : Bitfield<3, 1> { };
		};

		Tcnto::access_t cnt = { 0 };

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

		void update_cnt() { cnt = read<Tcnto>(); }

		Local(Genode::addr_t base);
	};

	/**
	 * Calculate amount of ticks per ms for specific input clock
	 *
	 * \param clock  input clock
	 */
	time_t static calc_ticks_per_ms(unsigned const clock) {
		return clock / (PRESCALER + 1) / (1 << DIV_MUX) / 1000; }

	Local          local;
	unsigned const ticks_per_ms;
	unsigned const cpu_id;

	Timer_driver(unsigned cpu_id);
};

#endif /* _TIMER_DRIVER_H_ */
