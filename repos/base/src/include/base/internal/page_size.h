/*
 * \brief  Page size utilities
 * \author Norman Feske
 * \date   2016-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__PAGE_SIZE_H_
#define _INCLUDE__BASE__INTERNAL__PAGE_SIZE_H_

#include <base/stdint.h>

namespace Genode {

	constexpr uint8_t PAGE_SIZE_LOG2 = 12;
	constexpr size_t  PAGE_SIZE      = 1 << PAGE_SIZE_LOG2;
	constexpr size_t  PAGE_MASK      = ~(PAGE_SIZE - 1UL);
}

#endif /* _INCLUDE__BASE__INTERNAL__PAGE_SIZE_H_ */
