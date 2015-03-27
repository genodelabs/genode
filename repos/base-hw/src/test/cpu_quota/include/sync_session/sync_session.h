/*
 * \brief  Sync session interface
 * \author Martin Stein
 * \date   2015-04-07
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SYNC_SESSION__SYNC_SESSION_H_
#define _SYNC_SESSION__SYNC_SESSION_H_

/* Genode includes */
#include <session/session.h>
#include <signal_session/signal_session.h>

namespace Sync
{
	using Genode::Signal_context_capability;

	struct Session;
}


struct Sync::Session : Genode::Session
{
	static const char *service_name() { return "Sync"; }

	virtual ~Session() { }

	/**
	 * Set the submission threshold of a synchronization signal
	 */
	virtual void threshold(unsigned id, unsigned threshold) = 0;

	/**
	 * Submit to a synchronization signal
	 */
	virtual void submit(unsigned id, Signal_context_capability sigc) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_threshold, void, threshold, unsigned, unsigned);
	GENODE_RPC(Rpc_submit, void, submit, unsigned, Signal_context_capability);

	GENODE_RPC_INTERFACE(Rpc_threshold, Rpc_submit);
};

#endif /* _SYNC_SESSION__SYNC_SESSION_H_ */
