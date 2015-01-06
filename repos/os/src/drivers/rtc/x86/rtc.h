/*
 * \brief  RTC driver interface
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _RTC_H_
#define _RTC_H_

#include <base/stdint.h>

namespace Rtc {

	/* Get real time in microseconds since 1970 */
	Genode::uint64_t get_time();
}

#endif /* _RTC_H_ */
