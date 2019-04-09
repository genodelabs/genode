/*
 * \brief  A duration type for both highly precise and long durations
 * \author Martin Stein
 * \date   2017-03-21
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/duration.h>

using namespace Genode;

void Duration::add(Microseconds us)
{
	if (us.value > ~(uint64_t)0 - _microseconds) {
		throw Overflow();
	}
	_microseconds += us.value;
}


void Duration::add(Milliseconds ms)
{
	if (ms.value > ~(uint64_t)0 / 1000) {
		throw Overflow();
	}
	add(Microseconds(ms.value * 1000));
}


bool Duration::less_than(Duration const &other) const
{
	return _microseconds < other._microseconds;
}


Microseconds Duration::trunc_to_plain_us() const
{
	return Microseconds(_microseconds);
}


Milliseconds Duration::trunc_to_plain_ms() const
{
	return Milliseconds(_microseconds / 1000);
}
