/*
 * \brief  Constants definitions for the x86 architecture.
 * \author Stefan Kalkowski
 * \date   2011-09-08
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPEC__X86__CPU__CONSTS_H_
#define _INCLUDE__SPEC__X86__CPU__CONSTS_H_

#include <base/stdint.h>

namespace X86 {

		enum Eflags_masks {
			CARRY       = 1 << 0,
			PARITY      = 1 << 2,
			ADJUST      = 1 << 4,
			ZERO        = 1 << 6,
			SIGN        = 1 << 7,
			TRAP        = 1 << 8,
			INT_ENABLE  = 1 << 9,
			DIRECTION   = 1 << 10,
			OVERFLOW    = 1 << 11,
			IOPL        = 3 << 12,
			NESTED_TASK = 1 << 14,
		};
}


namespace Abi {

	/**
	 * On x86 a call will result in a growth of the stack by machine word size
	 */
	static constexpr Genode::size_t stack_adjustment() { return sizeof(Genode::addr_t); }

	/**
	 * Do ABI specific initialization to a freshly created stack
	 *
	 * \param stack_top  top of the stack
	 */
	inline void init_stack(Genode::addr_t const stack_top)
	{
		/*
		 * The value at the top of the stack might get interpreted as return
		 * address of the thread start function by GDB, so we set it to 0.
		 */
		*(Genode::addr_t *)stack_top = 0;
	}
}

#endif /* _INCLUDE__SPEC__X86__CPU__CONSTS_H_ */
