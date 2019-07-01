/*
 * \brief  RTC driver interface
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__RTC__SPEC__X86__RTC_H_
#define _DRIVERS__RTC__SPEC__X86__RTC_H_


#include <base/env.h>
#include <rtc_session/rtc_session.h>

namespace Rtc {
	using namespace Genode;

	Timestamp get_time(Env &env);
	void      set_time(Env &env, Timestamp ts);
}

#endif /* _DRIVERS__RTC__SPEC__X86__RTC_H_ */
