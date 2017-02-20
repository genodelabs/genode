/*
 * \brief  Memory touch helpers
 * \author Norman Feske
 * \date   2007-04-29
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__TOUCH_H_
#define _INCLUDE__UTIL__TOUCH_H_

namespace Genode {

	/** Touch one byte at address read only */
	inline void touch_read(unsigned char const volatile *addr)
	{
		(void)*addr;
	}

	/** Touch one byte at address read/write */
	inline void touch_read_write(unsigned char volatile *addr)
	{
		unsigned char v = *addr;
		*addr = v;
	}
}

#endif /* _INCLUDE__UTIL__TOUCH_H_ */
