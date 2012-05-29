/*
 * \brief  Platform-timer base specific for base-hw and VEA9X4
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__SRC__DRIVERS__TIMER__HW__VEA9X4__PLATFORM_TIMER_BASE_H_
#define _OS__SRC__DRIVERS__TIMER__HW__VEA9X4__PLATFORM_TIMER_BASE_H_

/* Genode includes */
#include <io_mem_session/connection.h>
#include <drivers/timer/sp804_base.h>
#include <drivers/board/vea9x4.h>

/**
 * Platform-timer base specific for base-hw and VEA9X4
 */
class Platform_timer_base :
	public Genode::Io_mem_connection,
	public Genode::Sp804_base<Genode::Vea9x4::SP804_0_1_CLOCK>
{
	public:

		enum { IRQ = Genode::Vea9x4::SP804_0_1_IRQ };

		/**
		 * Constructor
		 */
		Platform_timer_base() :
			Io_mem_connection(Genode::Vea9x4::SP804_0_1_MMIO_BASE,
			                  Genode::Vea9x4::SP804_0_1_MMIO_SIZE),

			Sp804_base((Genode::addr_t)Genode::env()->rm_session()->
			                           attach(dataspace()))
		{ }
};

#endif /* _OS__SRC__DRIVERS__TIMER__HW__VEA9X4__PLATFORM_TIMER_BASE_H_ */

