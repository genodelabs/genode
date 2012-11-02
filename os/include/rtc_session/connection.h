/*
 * \brief  Connection to Rtc service
 * \author Markus Partheymueller
 * \date   2012-11-15
 */

/*
 * Copyright (C) 2012 Intel Corporation    
 * 
 * This file is part of the Genode OS framework and being contributed
 * under the terms and conditions of the Genode Contributor's Agreement
 * executed by Intel.
 */

#ifndef _INCLUDE__RTC_SESSION__CONNECTION_H_
#define _INCLUDE__RTC_SESSION__CONNECTION_H_

#include <rtc_session/client.h>
#include <base/connection.h>

namespace Rtc {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection() :
			Genode::Connection<Rtc::Session>(session("foo, ram_quota=4K")),
			Session_client(cap()) { }
	};
}

#endif
