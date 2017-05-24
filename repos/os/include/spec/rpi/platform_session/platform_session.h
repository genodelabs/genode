/*
 * \brief  Raspberry Pi specific platform session
 * \author Norman Feske
 * \date   2013-09-16
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
#include <dataspace/capability.h>
#include <platform/framebuffer_info.h>

namespace Platform {
	using namespace Genode;
	struct Session;
}


struct Platform::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Platform"; }

	enum { CAP_QUOTA = 2 };

	/**
	 * Setup framebuffer
	 *
	 * The 'info' argument serves as both input and output parameter. As input,
	 * it describes the desired properties of the framebuffer. In return, the
	 * method delivers the values that were actually taken into effect.
	 */
	virtual void setup_framebuffer(Framebuffer_info &info) = 0;

	enum Power {
		POWER_SDHCI   = 0,
		POWER_UART0   = 1,
		POWER_UART1   = 2,
		POWER_USB_HCD = 3,
		POWER_I2C0    = 4,
		POWER_I2C1    = 5,
		POWER_I2C2    = 6,
		POWER_SPI     = 7,
		POWER_CCP2TX  = 8,
	};

	/**
	 * Request power state
	 */
	virtual bool power_state(Power) = 0;

	/**
	 * Set power state
	 */
	virtual void power_state(Power, bool enable) = 0;

	enum Clock { CLOCK_EMMC = 1 };

	/**
	 * Request clock rate
	 */
	virtual uint32_t clock_rate(Clock) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_setup_framebuffer, void, setup_framebuffer, Framebuffer_info &);
	GENODE_RPC(Rpc_get_power_state, bool, power_state, Power);
	GENODE_RPC(Rpc_set_power_state, void, power_state, Power, bool);
	GENODE_RPC(Rpc_get_clock_rate, uint32_t, clock_rate, Clock);

	GENODE_RPC_INTERFACE(Rpc_setup_framebuffer, Rpc_set_power_state,
	                     Rpc_get_power_state, Rpc_get_clock_rate);
};

#endif /* _INCLUDE__PLATFORM_SESSION__PLATFORM_SESSION_H_ */
