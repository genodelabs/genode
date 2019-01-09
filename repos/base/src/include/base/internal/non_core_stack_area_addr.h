/*
 * \brief  Definition of the stack area outside of core
 * \author Norman Feske
 * \date   2019-01-09
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NON_CORE_STACK_AREA_ADDR_H_
#define _INCLUDE__BASE__INTERNAL__NON_CORE_STACK_AREA_ADDR_H_

namespace Genode
{
	static constexpr addr_t NON_CORE_STACK_AREA_ADDR = 0x40000000UL;
}

#endif /* _INCLUDE__BASE__INTERNAL__NON_CORE_STACK_AREA_ADDR_H_ */

