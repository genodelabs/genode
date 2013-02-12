/*
 * \brief  Client-side Rtc session interface
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

#ifndef _INCLUDE__RTC_SESSION__CLIENT_H_
#define _INCLUDE__RTC_SESSION__CLIENT_H_

#include <rtc_session/rtc_session.h>
#include <base/rpc_client.h>

namespace Rtc {

	class Session_client : public Genode::Rpc_client<Session>
	{
		public:
			Session_client(Genode::Capability<Session> cap)
			: Genode::Rpc_client<Session>(cap) {}

			Genode::uint64_t get_current_time()
			{
				return call<Rpc_get_current_time>();
			}
	};
}

#endif
