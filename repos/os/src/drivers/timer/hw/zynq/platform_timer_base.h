/*
 * \brief  Platform-timer base specific for base-hw and zynq
 * \author Johannes Schlatow
 * \date   2015-03-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__SRC__DRIVERS__TIMER__HW__ZYNQ__PLATFORM_TIMER_BASE_H_
#define _OS__SRC__DRIVERS__TIMER__HW__ZYNQ__PLATFORM_TIMER_BASE_H_

/* Genode includes */
#include <io_mem_session/connection.h>
#include <util/mmio.h>
#include <drivers/timer/ttc_base.h>
#include <drivers/board_base.h>

/**
 * Platform-timer base specific for base-hw and zynq
 * (see Xilinx ug585 "Zynq 7000 Technical Reference Manual")
 */
class Platform_timer_base :
	public Genode::Io_mem_connection,
	public Genode::Ttc_base<0, Genode::Board_base::CPU_1x_CLOCK>
{
	public:

		enum { IRQ = Genode::Board_base::TTC0_IRQ_0};

		/**
		 * Constructor
		 */
		Platform_timer_base() :
			Io_mem_connection(Genode::Board_base::TTC0_MMIO_BASE,
			                  Genode::Board_base::TTC0_MMIO_SIZE),

			Ttc_base((Genode::addr_t)Genode::env()->rm_session()->attach(dataspace()))
		{ }
};

#endif /* _OS__SRC__DRIVERS__TIMER__HW__ZYNQ__PLATFORM_TIMER_BASE_H_ */

