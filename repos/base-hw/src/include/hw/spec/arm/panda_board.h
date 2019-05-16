/*
 * \brief   Pandaboard specific definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__PANDA_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__PANDA_BOARD_H_

#include <base/stdint.h>
#include <drivers/defs/panda.h>
#include <drivers/uart/tl16c750.h>
#include <hw/spec/arm/boot_info.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

namespace Hw::Panda_board {
	using namespace Panda;
	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using Serial   = Genode::Tl16c750_uart;

	enum {
		UART_BASE  = TL16C750_3_MMIO_BASE,
		UART_CLOCK = TL16C750_CLOCK,
	};

	enum Panda_firmware_opcodes {
		CPU_ACTLR_SMP_BIT_RAISE =  0x25,
		L2_CACHE_SET_DEBUG_REG  = 0x100,
		L2_CACHE_ENABLE_REG     = 0x102,
		L2_CACHE_AUX_REG        = 0x109,
	};

	static inline void call_panda_firmware(Genode::addr_t func,
	                                       Genode::addr_t val)
	{
		register Genode::addr_t _func asm("r12") = func;
		register Genode::addr_t _val  asm("r0")  = val;
		asm volatile("dsb           \n"
		             "push {r1-r11} \n"
		             "smc  #0       \n"
		             "pop  {r1-r11} \n"
		             :: "r" (_func), "r" (_val) : "memory", "cc");
	}
}

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__PANDA_BOARD_H_ */
