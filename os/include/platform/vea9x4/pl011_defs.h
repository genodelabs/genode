/*
 * \brief  PL011 UART definitions for the RealView platform
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2011-05-27
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__PBXA9__PL011_DEFS_H_
#define _INCLUDE__PLATFORM__PBXA9__PL011_DEFS_H_

#include <platform/vea9x4/bus.h>
#include <drivers/board_base.h>

enum {

	/** Number of UARTs */
	PL011_NUM = 4,

	/**
	 * MMIO regions
	 */
	PL011_PHYS0 = SMB_CS7 + 0x9000,  /* base for UART 0 */
	PL011_PHYS1 = SMB_CS7 + 0xA000,  /* base for UART 1 */
	PL011_PHYS2 = SMB_CS7 + 0xB000,  /* base for UART 2 */
	PL011_PHYS3 = SMB_CS7 + 0xC000,  /* base for UART 3 */
	PL011_SIZE  = 0x1000,             /* size of each MMIO region */

	/**
	 * Interrupt lines
	 */
	PL011_IRQ0 = Genode::Board_base::PL011_0_IRQ, /* UART 0 */
	PL011_IRQ1 = Genode::Board_base::PL011_1_IRQ, /* UART 1 */
	PL011_IRQ2 = Genode::Board_base::PL011_2_IRQ, /* UART 2 */
	PL011_IRQ3 = Genode::Board_base::PL011_3_IRQ, /* UART 3 */

	/**
	 * UART baud rate configuration (precalculated)
	 *
	 * div  = 24000000 / 16 / baud rate
	 * IBRD = floor(div)
	 * FBRD = floor((div - IBRD) * 64 + 0.5)
	 */
	PL011_IBRD_115200 =  13, PL011_FBRD_115200 =  1,
	PL011_IBRD_19200  =  78, PL011_FBRD_19200  =  8,
	PL011_IBRD_9600   = 156, PL011_FBRD_9600   = 16,
};


static struct Pl011_uart {
	Genode::addr_t mmio_base;
	Genode::size_t mmio_size;
	int            irq_number;
} pl011_uart[PL011_NUM] = {
	{ PL011_PHYS0, PL011_SIZE, PL011_IRQ0 },
	{ PL011_PHYS1, PL011_SIZE, PL011_IRQ1 },
	{ PL011_PHYS2, PL011_SIZE, PL011_IRQ2 },
	{ PL011_PHYS3, PL011_SIZE, PL011_IRQ3 },
};

#endif /* _INCLUDE__PLATFORM__PBXA9__PL011_DEFS_H_ */
