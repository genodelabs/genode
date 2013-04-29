/*
 * \brief i.MX53 specific platform session
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com
 * \date 2013-04-29
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM_SESSION__PLATFORM_SESSION_H_
#define _INCLUDE__PLATFORM_SESSION__PLATFORM_SESSION_H_

#include <base/capability.h>
#include <base/rpc.h>

namespace Platform {

	struct Session : Genode::Session
	{
		enum Device { IPU };

		static const char *service_name() { return "Platform"; }

		virtual ~Session() { }

		virtual void enable(Device dev)  = 0;
		virtual void disable(Device dev) = 0;
		virtual void clock_rate(Device dev, unsigned long rate) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_enable,     void, enable,     Device);
		GENODE_RPC(Rpc_disable,    void, disable,    Device);
		GENODE_RPC(Rpc_clock_rate, void, clock_rate, Device, unsigned long);

		GENODE_RPC_INTERFACE(Rpc_enable, Rpc_disable, Rpc_clock_rate);
	};
}

#endif /* _INCLUDE__PLATFORM_SESSION__PLATFORM_SESSION_H_ */
