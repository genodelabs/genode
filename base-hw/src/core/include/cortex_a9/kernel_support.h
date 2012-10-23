/*
 * \brief  Parts of kernel support that are identical for all Cortex A9 systems
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORTEX_A9__KERNEL_SUPPORT_H_
#define _CORE__INCLUDE__CORTEX_A9__KERNEL_SUPPORT_H_

/* core includes */
#include <cortex_a9/cpu.h>
#include <cortex_a9/timer.h>
#include <cortex_a9/no_trustzone/pic.h>

/**
 * CPU driver
 */
class Cpu : public Cortex_a9::Cpu { };

namespace Kernel
{
	/**
	 * Programmable interrupt controller
	 */
	class Pic : public Cortex_a9_no_trustzone::Pic { };

	/**
	 * Kernel timer
	 */
	class Timer : public Cortex_a9::Timer { };
}

#endif /* _CORE__INCLUDE__CORTEX_A9__KERNEL_SUPPORT_H_ */

