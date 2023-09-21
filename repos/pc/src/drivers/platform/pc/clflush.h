/*
 * \brief  Helper for flushing translation-table entries from cache
 * \author Johannes Schlatow
 * \date   2023-09-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__CLFLUSH_H_
#define _SRC__DRIVERS__PLATFORM__CLFLUSH_H_

namespace Utils {
	inline void clflush(volatile void *addr)
	{
		asm volatile("clflush %0" : "+m" (*(volatile char *)addr));
	}
}

#endif /* _SRC__DRIVERS__PLATFORM__CLFLUSH_H_ */
