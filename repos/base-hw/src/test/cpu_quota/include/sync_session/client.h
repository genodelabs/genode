/*
 * \brief  Client-side Sync-session interface
 * \author Martin Stein
 * \date   2015-04-07
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SYNC_SESSION__CLIENT_H_
#define _SYNC_SESSION__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>

/* local includes */
#include <sync_session/capability.h>

namespace Sync
{
	using Genode::Rpc_client;

	struct Session_client;
}


struct Sync::Session_client : Rpc_client<Session>
{
	explicit Session_client(Session_capability session)
	: Rpc_client<Session>(session) { }

	void threshold(unsigned id, unsigned threshold) override {
		call<Rpc_threshold>(id, threshold); }

	void submit(unsigned id, Signal_context_capability sigc) override {
		call<Rpc_submit>(id, sigc); }
};

#endif /* _SYNC_SESSION__CLIENT_H_ */
