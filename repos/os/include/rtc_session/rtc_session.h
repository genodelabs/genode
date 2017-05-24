/*
 * \brief  Rtc session interface
 * \author Markus Partheymueller
 * \author Josef Soentgen
 * \date   2012-11-15
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RTC_SESSION__RTC_SESSION_H_
#define _INCLUDE__RTC_SESSION__RTC_SESSION_H_

#include <session/session.h>
#include <base/rpc.h>
#include <base/stdint.h>

namespace Rtc {
	struct Timestamp;
	struct Session;
}


struct Rtc::Timestamp
{
	unsigned microsecond;
	unsigned second;
	unsigned minute;
	unsigned hour;
	unsigned day;
	unsigned month;
	unsigned year;
};


struct Rtc::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Rtc"; }

	enum { CAP_QUOTA = 2 };

	virtual Timestamp current_time() = 0;

	GENODE_RPC(Rpc_current_time, Timestamp, current_time);
	GENODE_RPC_INTERFACE(Rpc_current_time);
};

#endif /* _INCLUDE__RTC_SESSION__RTC_SESSION_H_ */
