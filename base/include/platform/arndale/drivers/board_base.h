/*
 * \brief  Driver base for the InSignal Arndale 5 board
 * \author Martin stein
 * \date   2013-01-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__DRIVERS__BOARD_BASE_H_

namespace Genode
{
	/**
	 * Board driver base
	 */
	struct Board_base
	{
		enum
		{
			/* normal RAM */
			RAM_0_BASE = 0x40000000,
			RAM_0_SIZE = 0x80000000,

			/* device IO memory */
			MMIO_0_BASE = 0x10000000,
			MMIO_0_SIZE = 0x10000000,

			/* interrupt controller */
			GIC_CPU_MMIO_BASE = 0x10480000,
			GIC_CPU_MMIO_SIZE = 0x00010000,

			/* UART */
			UART_2_MMIO_BASE = 0x12C20000,
			UART_2_CLOCK     = 100000000,

			/* timer */
			PWM_MMIO_BASE = 0x12dd0000,
			PWM_CLOCK     = 66000000,
			PWM_IRQ_0     = 68,

			/* if board provides security extension */
			SECURITY_EXTENSION = 1,
		};
	};
}

#endif /* _INCLUDE__DRIVERS__BOARD_BASE_H_ */

