/*
 * \brief  Printable byte capacity
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__CAPACITY_H_
#define _MODEL__CAPACITY_H_

#include "types.h"

namespace Sculpt { struct Capacity; }

struct Sculpt::Capacity
{
	uint64_t value;

	void print(Output &out) const
	{
		uint64_t const KB = 1024, MB = 1024*KB, GB = 1024*MB;
		typedef String<64> Text;
		Text const text = (value > GB) ? Text((float)value/GB, " GiB")
		                : (value > MB) ? Text((float)value/MB, " MiB")
		                : (value > KB) ? Text((float)value/KB, " KiB")
		                :                Text(value,           " bytes");
		Genode::print(out, text);
	}
};

#endif /* _MODEL__CAPACITY_H_ */
