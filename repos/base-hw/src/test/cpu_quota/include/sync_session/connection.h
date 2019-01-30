/*
 * \brief  Connection to Sync service
 * \author Martin Stein
 * \date   2015-04-07
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SYNC_SESSION__CONNECTION_H_
#define _SYNC_SESSION__CONNECTION_H_

/* Genode includes */
#include <base/connection.h>
#include <base/rpc_client.h>

/* local includes */
#include <sync_session/sync_session.h>

namespace Sync { class Connection; }

struct Sync::Connection : public Genode::Connection<Session>,
                          public Genode::Rpc_client<Session>
{
		explicit Connection(Genode::Env &env)
		: Genode::Connection<Session>(env, session(env.parent(), "ram_quota=4K")),
		  Genode::Rpc_client<Session>(cap()) { }

		void threshold(unsigned threshold)            override { call<Rpc_threshold>(threshold); }
		void submit(Signal_context_capability signal) override { call<Rpc_submit>(signal); }
};

#endif /* _SYNC_SESSION__CONNECTION_H_ */
