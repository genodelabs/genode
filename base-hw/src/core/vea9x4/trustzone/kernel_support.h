/*
 * \brief  Kernel support specific for the Versatile VEA9X4
 * \author Stefan Kalkowski
 * \date   2012-10-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__VEA9X4__TRUSTZONE__KERNEL_SUPPORT_H_
#define _SRC__CORE__VEA9X4__TRUSTZONE__KERNEL_SUPPORT_H_

/* Core includes */
#include <cortex_a9/cpu.h>
#include <cortex_a9/timer.h>
#include <vea9x4_trustzone/pic.h>

/**
 * CPU driver
 */
class Cpu : public Cortex_a9::Cpu { };

namespace Kernel
{
	/**
	 * Programmable interrupt controller
	 */
	class Pic : public Vea9x4_trustzone::Pic { };

	/**
	 * Kernel timer
	 */
	class Timer : public Cortex_a9::Timer { };
}


#endif /* _SRC__CORE__VEA9X4__TRUSTZONE__KERNEL_SUPPORT_H_ */

