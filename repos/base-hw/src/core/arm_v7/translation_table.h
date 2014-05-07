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

#ifndef _ARM_V7__TRANSLATION_TABLE_H_
#define _ARM_V7__TRANSLATION_TABLE_H_

/* core includes */
#include <arm/short_translation_table.h>

template <typename T>
static typename T::access_t
Arm::memory_region_attr(Page_flags const & flags)
{
	typedef typename T::Tex Tex;
	typedef typename T::C C;
	typedef typename T::B B;
	if (flags.device) { return Tex::bits(2); }
	if (flags.cacheable) { return Tex::bits(5) | B::bits(1); }
	return Tex::bits(6) | C::bits(1);
}

#endif /* _ARM_V7__TRANSLATION_TABLE_H_ */
