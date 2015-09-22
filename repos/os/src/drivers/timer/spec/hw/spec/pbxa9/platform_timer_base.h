/*
 * \brief  Platform-timer base specific for base-hw and PBXA9
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__SRC__DRIVERS__TIMER__HW__PBXA9__PLATFORM_TIMER_BASE_H_
#define _OS__SRC__DRIVERS__TIMER__HW__PBXA9__PLATFORM_TIMER_BASE_H_

/* Genode includes */
#include <io_mem_session/connection.h>
#include <sp804_base.h>
#include <drivers/board_base.h>

/**
 * Platform-timer base specific for base-hw and PBXA9
 */
class Platform_timer_base :
	public Genode::Io_mem_connection,
	public Genode::Sp804_base<Genode::Board_base::SP804_0_1_CLOCK>
{
	public:

		enum { IRQ = Genode::Board_base::SP804_0_1_IRQ };

		/**
		 * Constructor
		 */
		Platform_timer_base() :
			Io_mem_connection(Genode::Board_base::SP804_0_1_MMIO_BASE,
			                  Genode::Board_base::SP804_0_1_MMIO_SIZE),

			Sp804_base((Genode::addr_t)Genode::env()->rm_session()->
			                           attach(dataspace()))
		{ }
};

#endif /* _OS__SRC__DRIVERS__TIMER__HW__PBXA9__PLATFORM_TIMER_BASE_H_ */

