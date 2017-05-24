/*
 * \brief i.MX53 specific platform session
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com
 * \date 2013-04-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLATFORM_SESSION__PLATFORM_SESSION_H_
#define _INCLUDE__PLATFORM_SESSION__PLATFORM_SESSION_H_

#include <base/capability.h>
#include <base/rpc.h>

namespace Platform { struct Session; }


struct Platform::Session : Genode::Session
{
	enum Device {
		IPU,
		I2C_2,
		I2C_3,
		BUTTONS,
		PWM,
	};

	enum Board_revision {
		SMD = 2,  /* Freescale i.MX53 SMD Tablet */
		QSB = 3,  /* Freescale i.MX53 low-cost Quickstart board */
		UNKNOWN,
	};

	/**
	 * \noapi
	 */
	static const char *service_name() { return "Platform"; }

	enum { CAP_QUOTA = 2 };

	virtual ~Session() { }

	virtual void enable(Device dev)   = 0;
	virtual void disable(Device dev)  = 0;
	virtual void clock_rate(Device dev, unsigned long rate) = 0;
	virtual Board_revision revision() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_enable,     void, enable,     Device);
	GENODE_RPC(Rpc_disable,    void, disable,    Device);
	GENODE_RPC(Rpc_clock_rate, void, clock_rate, Device, unsigned long);
	GENODE_RPC(Rpc_revision, Board_revision, revision);

	GENODE_RPC_INTERFACE(Rpc_enable, Rpc_disable, Rpc_clock_rate,
	                     Rpc_revision);
};

#endif /* _INCLUDE__PLATFORM_SESSION__PLATFORM_SESSION_H_ */
