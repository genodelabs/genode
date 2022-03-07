/*
 * \brief  Time-taking defintions for Genode test cases
 * \author Johannes Schlatow
 * \date   2022-03-07
 *
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__TEST__CACHE__GENODE_TIME_H_
#define _SRC__TEST__CACHE__GENODE_TIME_H_

#include <trace/timestamp.h>

using namespace Genode::Trace;
using namespace Genode;

struct Duration { unsigned long value; };

struct Time
{
	Timestamp _ts { timestamp() };

	static Duration duration(Time t1, Time t2)
	{
		Timestamp diff = t2._ts - t1._ts;
		if (t2._ts < t1._ts) diff--;

		return Duration { (unsigned long)diff };
	}
};

#endif /* _SRC__TEST__CACHE__GENODE_TIME_H_ */
