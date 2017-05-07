/*
 * \brief  Sync session interface
 * \author Martin Stein
 * \date   2015-04-07
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SYNC_SESSION__SYNC_SESSION_H_
#define _SYNC_SESSION__SYNC_SESSION_H_

/* Genode includes */
#include <base/capability.h>
#include <session/session.h>
#include <base/signal.h>

namespace Sync
{
	using Genode::Signal_context_capability;

	struct Session;
	using  Session_capability = Genode::Capability<Session>;
}

struct Sync::Session : Genode::Session
{
	static const char *service_name() { return "Sync"; }

	enum { CAP_QUOTA = 2 };

	virtual ~Session() { }

	virtual void threshold(unsigned threshold) = 0;
	virtual void submit(Signal_context_capability signal) = 0;

	GENODE_RPC(Rpc_threshold, void, threshold, unsigned);
	GENODE_RPC(Rpc_submit, void, submit, Signal_context_capability);

	GENODE_RPC_INTERFACE(Rpc_threshold, Rpc_submit);
};

#endif /* _SYNC_SESSION__SYNC_SESSION_H_ */
