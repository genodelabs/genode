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

#ifndef _DRIVERS__RTC__SPEC__X86__RTC_H_
#define _DRIVERS__RTC__SPEC__X86__RTC_H_

#include <base/stdint.h>
#include <rtc_session/rtc_session.h>

namespace Rtc {

	Timestamp get_time();
}

#endif /* _DRIVERS__RTC__SPEC__X86__RTC_H_ */
