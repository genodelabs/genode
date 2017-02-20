/*
 * \brief  Client-side timer session interface
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2006-05-31
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TIMER_SESSION__CLIENT_H_
#define _INCLUDE__TIMER_SESSION__CLIENT_H_

#include <timer_session/capability.h>
#include <base/rpc_client.h>

namespace Timer { struct Session_client; }


struct Timer::Session_client : Genode::Rpc_client<Session>
{
	explicit Session_client(Session_capability session)
	: Genode::Rpc_client<Session>(session) { }

	void trigger_once(unsigned us) override { call<Rpc_trigger_once>(us); }

	void trigger_periodic(unsigned us) override { call<Rpc_trigger_periodic>(us); }

	void sigh(Signal_context_capability sigh) override { call<Rpc_sigh>(sigh); }

	unsigned long elapsed_ms() const override { return call<Rpc_elapsed_ms>(); }
};

#endif /* _INCLUDE__TIMER_SESSION__CLIENT_H_ */
