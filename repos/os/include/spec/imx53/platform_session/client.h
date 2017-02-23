/*
 * \brief i.MX53 specific platform session client side
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com
 * \date 2013-04-29
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


struct Platform::Client : Genode::Rpc_client<Session>
{
	explicit Client(Capability<Session> session)
	: Genode::Rpc_client<Session>(session) { }

	void enable(Device dev) override { call<Rpc_enable>(dev); }
	void disable(Device dev) override { call<Rpc_disable>(dev); }
	void clock_rate(Device dev, unsigned long rate) override {
		call<Rpc_clock_rate>(dev, rate); }
	Board_revision revision() override { return call<Rpc_revision>(); }
};

#endif /* _INCLUDE__PLATFORM_SESSION__CLIENT_H_ */
