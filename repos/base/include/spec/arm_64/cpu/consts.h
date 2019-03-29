/*
 * \brief  Constants definitions for ARM 64-bit.
 * \author Stefan Kalkowski
 * \date   2019-03-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__CPU__CONSTS_H_
#define _INCLUDE__SPEC__ARM_64__CPU__CONSTS_H_

#include <base/stdint.h>

namespace Abi {

	/*
	 * On ARM we align the stack top to 16-byte.
	 */
	inline Genode::addr_t stack_align(Genode::addr_t addr) {
		return (addr & ~0xf); }

	/**
	 * Do ABI specific initialization to a freshly created stack
	 */
	inline void init_stack(Genode::addr_t) { }
}

#endif /* _INCLUDE__SPEC__ARM_64__CPU__CONSTS_H_ */
