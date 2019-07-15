/*
 * \brief  Rtc session interface
 * \author Markus Partheymueller
 * \author Josef Soentgen
 * \date   2012-11-15
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RTC_SESSION__RTC_SESSION_H_
#define _INCLUDE__RTC_SESSION__RTC_SESSION_H_

#include <session/session.h>
#include <base/rpc.h>
#include <base/stdint.h>
#include <base/signal.h>

namespace Rtc {
	struct Timestamp;
	struct Session;
}


/*
 * RTC value
 */
struct Rtc::Timestamp
{
	unsigned microsecond;
	unsigned second;
	unsigned minute;
	unsigned hour;
	unsigned day;
	unsigned month;
	unsigned year;

	void print(Genode::Output &out) const
	{
		Genode::print(out, year, "-", month, "-", day, " ",
		              hour, ":", minute, ":", second);
	}
};


struct Rtc::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Rtc"; }

	enum { CAP_QUOTA = 2 };

	/***********************
	 ** Session interface **
	 ***********************/

	/**
	 * Register set signal handler
	 *
	 * \param sigh  signal handler that is called when the RTC has
	 *              been set
	 */
	virtual void set_sigh(Genode::Signal_context_capability sigh) = 0;

	/**
	 * Query current time
	 *
	 * \return  RTC value as structed timestamp
	 */
	virtual Timestamp current_time() = 0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_set_sigh, void, set_sigh,
	           Genode::Signal_context_capability);
	GENODE_RPC(Rpc_current_time, Timestamp, current_time);
	GENODE_RPC_INTERFACE(Rpc_set_sigh, Rpc_current_time);
};

#endif /* _INCLUDE__RTC_SESSION__RTC_SESSION_H_ */
