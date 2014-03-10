/*
 * \brief  Core-internal utilities
 * \author Stefan Kalkowski
 * \date   2014-03-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__UTIL_H_
#define _CORE__INCLUDE__UTIL_H_

namespace Genode {
	constexpr size_t get_page_size_log2() { return 12; }
	constexpr size_t get_page_size()      { return 1 << get_page_size_log2(); }
}

#endif /* _CORE__INCLUDE__UTIL_H_ */
