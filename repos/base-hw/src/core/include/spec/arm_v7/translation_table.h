/*
 * \brief   Armv7 translation table definitions for core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TRANSLATION_TABLE_H_
#define _TRANSLATION_TABLE_H_

/* core includes */
#include <spec/arm/short_translation_table.h>

template <typename T>
static typename T::access_t
Genode::arm_memory_region_attr(Page_flags const & flags)
{
	typedef typename T::Tex Tex;
	typedef typename T::C C;
	typedef typename T::B B;
	if (flags.device) { return Tex::bits(2); }

	switch (flags.cacheable) {
	case CACHED:         return Tex::bits(5) | B::bits(1);
	case WRITE_COMBINED: return B::bits(1);
	case UNCACHED:       return Tex::bits(1);
	}
	return 0;
}

#endif /* _TRANSLATION_TABLE_H_ */
