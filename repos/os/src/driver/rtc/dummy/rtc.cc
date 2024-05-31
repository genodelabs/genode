/*
 * \brief  RTC dummy driver
 * \author Stefan Kalkowski
 * \date   2021-02-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <rtc.h>

Rtc::Timestamp & current_ts()
{
	static Rtc::Timestamp cur {};
	return cur;
}


Rtc::Timestamp Rtc::get_time(Env &) { return current_ts(); }


void Rtc::set_time(Env &, Timestamp ts)
{
	Timestamp & cur = current_ts();
	cur = ts;
}
