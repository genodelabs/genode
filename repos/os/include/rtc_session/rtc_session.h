/*
 * \brief  Rtc session interface
 * \author Markus Partheymueller
 * \date   2012-11-15
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RTC_SESSION__RTC_SESSION_H_
#define _INCLUDE__RTC_SESSION__RTC_SESSION_H_

#include <session/session.h>
#include <base/rpc.h>
#include <base/stdint.h>

namespace Rtc {

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Rtc"; }

		/**
		 * Get microseconds elapsed since 1.1.1970 UTC.
		 */
		virtual Genode::uint64_t get_current_time() = 0;

		GENODE_RPC(Rpc_get_current_time, Genode::uint64_t, get_current_time);
		GENODE_RPC_INTERFACE(Rpc_get_current_time);
	};
}

#endif /* _INCLUDE__RTC_SESSION__RTC_SESSION_H_ */
