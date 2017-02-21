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
		asm volatile("dsb; smc #0" :: "r" (_func), "r" (_val) :
		             "memory", "cc", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
		             "r8", "r9", "r10", "r11");
	}
}

#endif /* _SRC__LIB__HW__SPEC__ARM__PANDA_TRUSTZONE_FIRMWARE_H_ */
