/*
 * \brief  Connection to Rtc service
 * \author Markus Partheymueller
 * \date   2012-11-15
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RTC_SESSION__CONNECTION_H_
#define _INCLUDE__RTC_SESSION__CONNECTION_H_

#include <rtc_session/client.h>
#include <base/connection.h>

namespace Rtc { struct Connection; }


struct Rtc::Connection : Genode::Connection<Session>, Session_client
{
	Connection(Genode::Env &env, Label const &label = Label())
	:
		Genode::Connection<Rtc::Session>(env, label, Ram_quota { 8*1024 }, Args()),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__RTC_SESSION__CONNECTION_H_ */
