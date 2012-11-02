/*
 * \brief  Rtc session interface
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

#ifndef _INCLUDE__RTC_SESSION__RTC_SESSION_H_
#define _INCLUDE__RTC_SESSION__RTC_SESSION_H_

#include <session/session.h>
#include <base/rpc.h>
#include <base/signal.h>
#include <base/stdint.h>

namespace Rtc {

	struct Session : Genode::Session
	{
		static const char *service_name() {
			return "Rtc";
		}

		virtual Genode::uint64_t get_current_time() = 0;

		GENODE_RPC(Rpc_get_current_time, Genode::uint64_t, get_current_time);
		GENODE_RPC_INTERFACE(Rpc_get_current_time);
	};

}

#endif
