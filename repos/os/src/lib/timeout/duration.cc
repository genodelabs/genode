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
#include <os/duration.h>

using namespace Genode;


void Duration::_raise_hours(unsigned long hours)
{
	unsigned long const old_hours = _hours;
	_hours += hours;
	if (_hours < old_hours) {
		throw Overflow(); }
}


void Duration::operator += (Microseconds us)
{
	_microseconds += us.value;
	if (_microseconds > US_PER_HOUR) {
		_microseconds -= US_PER_HOUR;
		_raise_hours(1);
	}
}


void Duration::operator += (Milliseconds ms)
{
	/* filter out hours if any */
	if (ms.value >= MS_PER_HOUR) {
		unsigned long const hours = ms.value / MS_PER_HOUR;
		_raise_hours(hours);
		ms.value -= hours * MS_PER_HOUR;
	}
	/* add the rest as microseconds value */
	*this += Microseconds(ms.value * US_PER_MS);
}


bool Duration::operator < (Duration &other) const
{
	if (_hours != other._hours) {
		return _hours < other._hours; }

	if (_microseconds != other._microseconds) {
		return _microseconds < other._microseconds; }

	return false;
}


Microseconds Duration::trunc_to_plain_us() const
{
	return Microseconds(_microseconds + (_hours ? _hours * US_PER_HOUR : 0));
}
