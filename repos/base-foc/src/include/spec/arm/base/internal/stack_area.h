/*
 * \brief  Stack area layout parameters
 * \author Norman Feske
 * \date   2016-03-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__STACK_AREA_H_
#define _INCLUDE__BASE__INTERNAL__STACK_AREA_H_

#include <base/stdint.h>

namespace Genode {

	/*
	 * The base address of the context area differs between ARM and x86 because
	 * roottask on Fiasco.OC uses identity mappings. The virtual address range
	 * of the stack area must not overlap with physical memory. We pick an
	 * address range that lies outside of the RAM of the currently supported
	 * ARM platforms.
	 */
	static constexpr addr_t stack_area_virtual_base() { return 0x20000000UL; }
	static constexpr addr_t stack_area_virtual_size() { return 0x10000000UL; }
	static constexpr addr_t stack_virtual_size()      { return 0x00100000UL; }
}

#endif /* _INCLUDE__BASE__INTERNAL__STACK_AREA_H_ */
