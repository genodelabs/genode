/*
 * \brief  PL011 UART definitions for the Versatile Platform Baseboard 926
 * \author Christian Helmuth
 * \date   2011-11-09
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__VPB926__PL011_DEFS_H_
#define _INCLUDE__PLATFORM__VPB926__PL011_DEFS_H_

#include <base/stdint.h>

#warning pl011 untested on vpb926

enum {

	/** Number of UARTs */
	PL011_NUM = 4,

	/**
	 * MMIO regions
	 */
	PL011_PHYS0 = 0x101f1000,  /* base for UART 0 */
	PL011_PHYS1 = 0x101f2000,  /* base for UART 1 */
	PL011_PHYS2 = 0x101f3000,  /* base for UART 2 */
	PL011_PHYS3 = 0x10009000,  /* base for UART 3 */
	PL011_SIZE  = 0x1000,      /* size of each MMIO region */

	/**
	 * Interrupt lines
	 */
	/* FIXME find correct vectors */
	PL011_IRQ0 = 12,      /* UART 0 on primary */
	PL011_IRQ1 = 13,      /* UART 1 on primary */
	PL011_IRQ2 = 14,      /* UART 2 on primary */
	PL011_IRQ3 =  6,      /* UART 3 on secondary */

	/**
	 * UART baud rate configuration (precalculated)
	 *
	 * div  = 24000000 / 16 / baud rate
	 * IBRD = floor(div)
	 * FBRD = floor((div - IBRD) * 64 + 0.5)
	 */
	/* FIXME calculate correct / check values */
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

#endif /* _INCLUDE__PLATFORM__VPB926__PL011_DEFS_H_ */
