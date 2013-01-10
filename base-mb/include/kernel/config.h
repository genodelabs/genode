/*
 * \brief  Configuration of kernel features
 * \author Martin stein
 * \date   24-06-2010
 */

/*
 * Copyright (C) 24-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__KERNEL__CONFIG_H_
#define _INCLUDE__KERNEL__CONFIG_H_

#include <cpu/config.h>

namespace Kernel 
{
	enum { 
		SCHEDULING_MS_INTERVAL  = 10,
		SCHEDULING_TIMER_BASE   = Cpu::XPS_TIMER_0_BASE,
		SCHEDULING_TIMER_IRQ    = Cpu::XPS_TIMER_0_IRQ,

		DEFAULT_PAGE_SIZE_LOG2 = Cpu::_4KB_SIZE_LOG2,
	};

	typedef Cpu::uint8_t Thread_id;
	typedef Cpu::uint8_t Protection_id;

	enum{
		MIN_THREAD_ID = 1,
		MAX_THREAD_ID = 64,

		MIN_PROTECTION_ID = 1,
		MAX_PROTECTION_ID = 64,

		INVALID_THREAD_ID     = 0,
		INVALID_PROTECTION_ID = 0 };
}

namespace Roottask
{
	enum {
		MAIN_STACK_SIZE = 1024*1024*Cpu::WORD_SIZE,
		MAIN_THREAD_ID  = 2,

		PROTECTION_ID   = 1,
	};
}

namespace User 
{
	enum {
		UART_BASE = Cpu::XPS_UARTLITE_BASE,
		UART_IRQ  = Cpu::XPS_UARTLITE_IRQ,

		IO_MEM_BASE = 0x70000000,
		IO_MEM_SIZE = 0x10000000,

		XPS_TIMER_0_BASE = 0x70000000,
		XPS_TIMER_0_IRQ  = 4,

		MIN_IRQ = 4,
		MAX_IRQ = 31,

		MIN_PROTECTION_ID = Roottask::PROTECTION_ID+1,
		MAX_PROTECTION_ID = Kernel::MAX_PROTECTION_ID,

		MIN_THREAD_ID = Roottask::MAIN_THREAD_ID+1,
		MAX_THREAD_ID = Kernel::MAX_THREAD_ID,

		VADDR_BASE = 0 + 1*(1<<Kernel::DEFAULT_PAGE_SIZE_LOG2),
		VADDR_SIZE = 0xf0000000,
	};
}

#endif /* _INCLUDE__KERNEL__CONFIG_H_ */
