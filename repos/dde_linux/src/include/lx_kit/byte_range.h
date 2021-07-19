/*
 * \brief  Lx_kit byte-range utility
 * \author Norman Feske
 * \date   2021-07-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__BYTE_RANGE_H_
#define _LX_KIT__BYTE_RANGE_H_

#include <base/stdint.h>

namespace Lx_kit {

	using namespace Genode;

	struct Byte_range;
}


struct Lx_kit::Byte_range
{
	addr_t start;
	size_t size;

	bool intersects(Byte_range const &other) const
	{
		if (start > other.start + other.size - 1)
			return false;

		if (start + size - 1 < other.start)
			return false;

		return true;
	}
};

#endif /* _LX_KIT__BYTE_RANGE_H_ */
