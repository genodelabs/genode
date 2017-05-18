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
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env, char const *label = "")
	:
		Genode::Connection<Rtc::Session>(
			env, session(env.parent(), "ram_quota=8K, label=\"%s\"", label)),
		Session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection() __attribute__((deprecated))
	:
		Genode::Connection<Rtc::Session>(session("ram_quota=8K")),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__RTC_SESSION__CONNECTION_H_ */
