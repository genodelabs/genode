/*
 * \brief  Pandaboard's TrustZone firmware frontend
 * \author Stefan Kalkowski
 * \date   2017-02-02
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__PANDA_TRUSTZONE_FIRMWARE_H_
#define _SRC__LIB__HW__SPEC__ARM__PANDA_TRUSTZONE_FIRMWARE_H_

#include <base/stdint.h>

namespace Hw {

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

#endif /* _SRC__LIB__HW__SPEC__ARM__PANDA_TRUSTZONE_FIRMWARE_H_ */
