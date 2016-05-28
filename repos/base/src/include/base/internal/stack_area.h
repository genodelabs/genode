/*
 * \brief  Stack area layout parameters
 * \author Norman Feske
 * \date   2016-03-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__STACK_AREA_H_
#define _INCLUDE__BASE__INTERNAL__STACK_AREA_H_

#include <base/stdint.h>

namespace Genode {

	static constexpr addr_t stack_area_virtual_base() { return 0x40000000UL; }
	static constexpr addr_t stack_area_virtual_size() { return 0x10000000UL; }
	static constexpr addr_t stack_virtual_size()      { return 0x00100000UL; }
}

#endif /* _INCLUDE__BASE__INTERNAL__STACK_AREA_H_ */
