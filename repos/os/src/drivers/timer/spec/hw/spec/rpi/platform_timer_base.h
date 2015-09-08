/*
 * \brief  User-level timer driver for Raspberry Pi
 * \author Norman Feske
 * \date   2013-04-11
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _HW__RPI__PLATFORM_TIMER_BASE_H_
#define _HW__RPI__PLATFORM_TIMER_BASE_H_

/* Genode includes */
#include <os/attached_io_mem_dataspace.h>
#include <sp804_base.h>
#include <drivers/board_base.h>

/*
 * On the BCM2835, the timer is driven by the APB clock (250 MHz). The prescale
 * register (not present in the normal SP804) has a reset value of 126. Hence,
 * the effective timer clock is 1,984 MHz.
 *
 * The timer device is on the same physical page as the IRQ controller. Hence,
 * we open an IO_MEM session with a range smaller than page size as argument.
 * The dataspace base address will correspond to 0x2000b000.
 */
enum { TIMER_MMIO_BASE   = 0x2000b400,
       TIMER_MMIO_SIZE   = 0x100,
       TIMER_CLOCK       = 1984*1000 };

struct Platform_timer_base
:
	Genode::Attached_io_mem_dataspace,
	Genode::Sp804_base<TIMER_CLOCK>
{
	enum { IRQ = Genode::Board_base::TIMER_IRQ };

	Platform_timer_base() :
		Attached_io_mem_dataspace(TIMER_MMIO_BASE, TIMER_MMIO_SIZE),
		Sp804_base((Genode::addr_t)local_addr<void>())
	{ }
};

#endif /* _HW__RPI__PLATFORM_TIMER_BASE_H_ */

