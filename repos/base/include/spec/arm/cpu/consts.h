/*
 * \brief  Constants definitions for the ARM architecture.
 * \author Sebastian Sumpf
 * \date   2014-02-20
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__CPU__CONSTS_H_
#define _INCLUDE__SPEC__ARM__CPU__CONSTS_H_

#include <base/stdint.h>

namespace Abi {

	/*
	 * On ARM we align the stack top to 16-byte. As a call (or branch) will not
	 * change the stack pointer, we need no further stack adjustment.
	 */
	inline Genode::addr_t stack_align(Genode::addr_t addr) {
		return (addr & ~0xf); }

	/**
	 * Do ABI specific initialization to a freshly created stack
	 */
	inline void init_stack(Genode::addr_t) { }
}

#endif /* _INCLUDE__SPEC__ARM__CPU__CONSTS_H_ */
