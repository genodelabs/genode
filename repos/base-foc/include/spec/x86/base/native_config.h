/*
 * \brief  Platform-specific stack area definitions
 * \author Stefan Kalkowski
 * \date   2014-01-24
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPEC__X86__BASE__NATIVE_CONFIG_H_
#define _INCLUDE__SPEC__X86__BASE__NATIVE_CONFIG_H_

#include <base/stdint.h>

namespace Genode {

	struct Native_config
	{
		/**
		 * Stack area configuration
		 */
		static constexpr addr_t stack_area_virtual_base() {
			return 0x40000000UL; }
		static constexpr addr_t stack_area_virtual_size() {
			return 0x10000000UL; }

		/**
		 * Size of virtual address region holding the stack of one thread
		 */
		static constexpr addr_t stack_virtual_size() { return 0x00100000UL; }
	};
}

#endif /* _INCLUDE__SPEC__X86__BASE__NATIVE_CONFIG_H_ */
