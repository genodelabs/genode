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

namespace Genode
{
	constexpr uint8_t get_page_size_log2() { return 12; }
	constexpr size_t  get_page_size()      { return 1 << get_page_size_log2(); }
}

#endif /* _INCLUDE__BASE__INTERNAL__PAGE_SIZE_H_ */
