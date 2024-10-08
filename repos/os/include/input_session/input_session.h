/*
 * \brief  Input session interface
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__INPUT_SESSION__INPUT_SESSION_H_
#define _INCLUDE__INPUT_SESSION__INPUT_SESSION_H_

#include <dataspace/capability.h>
#include <base/rpc_server.h>
#include <session/session.h>
#include <base/signal.h>

namespace Input {

	using namespace Genode;

	struct Session;
}


struct Input::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Input"; }

	/*
	 * An input session consumes a dataspace capability for the server's
	 * session-object allocation, a dataspace capability for the input
	 * buffer, and its session capability.
	 */
	static constexpr unsigned CAP_QUOTA = 3;

	virtual ~Session() { }

	/**
	 * Return capability to event buffer dataspace
	 */
	virtual Dataspace_capability dataspace() = 0;

	/**
	 * Request input state
	 *
	 * \return  true if new events are available
	 */
	virtual bool pending() const = 0;

	/**
	 * Flush pending events to event buffer
	 *
	 * \return  number of flushed events
	 */
	virtual int flush() = 0;

	/**
	 * Register signal handler to be notified on arrival of new input
	 */
	virtual void sigh(Signal_context_capability) = 0;

	/**
	 * Express intent to receive relative pointer events exclusively
	 */
	virtual void exclusive(bool) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_pending,   bool, pending);
	GENODE_RPC(Rpc_flush,     int,  flush);
	GENODE_RPC(Rpc_sigh,      void, sigh, Signal_context_capability);
	GENODE_RPC(Rpc_exclusive, void, exclusive, bool);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_pending, Rpc_flush, Rpc_sigh,
	                     Rpc_exclusive);
};

#endif /* _INCLUDE__INPUT_SESSION__INPUT_SESSION_H_ */
