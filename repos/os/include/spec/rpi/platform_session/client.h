/*
 * \brief  Raspberry Pi specific platform session client side
 * \author Norman Feske
 * \date   2013-09-16
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLATFORM_SESSION__CLIENT_H_
#define _INCLUDE__PLATFORM_SESSION__CLIENT_H_

#include <base/capability.h>
#include <base/rpc_client.h>
#include <platform_session/platform_session.h>

namespace Platform { struct Client; }

struct Platform::Client : Genode::Rpc_client<Platform::Session>
{
	explicit Client(Capability<Session> session)
	: Genode::Rpc_client<Session>(session) { }

	void setup_framebuffer(Framebuffer_info &info) override {
		call<Rpc_setup_framebuffer>(info); }

	bool power_state(Power power) override {
		return call<Rpc_get_power_state>(power); }

	void power_state(Power power, bool enable) override {
		call<Rpc_set_power_state>(power, enable); }

	uint32_t clock_rate(Clock clock) {
		return call<Rpc_get_clock_rate>(clock); }
};

#endif /* _INCLUDE__PLATFORM_SESSION__CLIENT_H_ */
