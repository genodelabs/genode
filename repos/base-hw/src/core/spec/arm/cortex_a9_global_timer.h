/*
 * \brief  Global timer implementation specific to Cortex A9
 * \author Johannes Schlatow
 * \date   2023-01-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__CORE__SPEC__ARM__CORTEX_A9_GLOBAL_TIMER_H_
#define _SRC__CORE__SPEC__ARM__CORTEX_A9_GLOBAL_TIMER_H_

/* Genode includes */
#include <util/mmio.h>

/* base-hw includes */
#include <kernel/types.h>

namespace Board { class Timer; }


/**
 * Timer driver for core
 */
struct Board::Timer : Genode::Mmio<0x18>
{
	/**
	 * Counter value registers
	 */
	struct Counter : Register_array<0x0, 32, 2, 32> { };

	/**
	 * Timer control register
	 */
	struct Control : Register<0x8, 32>
	{
		struct Timer_enable   : Bitfield<0,1> { }; /* enable counting */
		struct Comp_enable    : Bitfield<1,1> { };
		struct Irq_enable     : Bitfield<2,1> { }; /* unmask interrupt */
		struct Auto_increment : Bitfield<3,1> { };
		struct Prescaler      : Bitfield<8,8> { };
	};

	/**
	 * Timer interrupt status register
	 */
	struct Interrupt_status : Register<0xc, 32>
	{
		struct Event : Bitfield<0,1> { }; /* if counter hit zero */
	};

	/**
	 * Comparator registers
	 */
	struct Comparator : Register_array<0x10, 32, 2, 32> { };

	Kernel::time_t current_ticks() const;

	Timer(unsigned);

	void init();
};

#endif /* _SRC__CORE__SPEC__ARM__CORTEX_A9_GLOBAL_TIMER_H_ */
