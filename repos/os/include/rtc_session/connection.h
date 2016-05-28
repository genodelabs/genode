/*
 * \brief  Connection to Rtc service
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
	Connection(Genode::Env &env)
	:
		Genode::Connection<Rtc::Session>(env, session(env.parent(), "ram_quota=4K")),
		Session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection()
	:
		Genode::Connection<Rtc::Session>(session("ram_quota=4K")),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__RTC_SESSION__CONNECTION_H_ */
