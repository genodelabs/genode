/*
 * \brief  Client-side RTC session interface
 * \author Markus Partheymueller
 * \date   2012-11-15
 */

/*
 * Copyright (C) 2012      Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RTC_SESSION__CLIENT_H_
#define _INCLUDE__RTC_SESSION__CLIENT_H_

#include <rtc_session/rtc_session.h>
#include <base/rpc_client.h>

namespace Rtc { struct Session_client; }


struct Rtc::Session_client : Genode::Rpc_client<Session>
{
	Session_client(Genode::Capability<Session> cap)
	: Genode::Rpc_client<Session>(cap) {}

	Timestamp current_time() { return call<Rpc_current_time>(); }
};

#endif /* _INCLUDE__RTC_SESSION__CLIENT_H_ */
