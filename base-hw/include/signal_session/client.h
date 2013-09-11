/*
 * \brief  Client-side implementation of the signal session interface
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__CLIENT_H_
#define _INCLUDE__SIGNAL_SESSION__CLIENT_H_

/* Genode includes */
#include <signal_session/capability.h>
#include <signal_session/signal_session.h>
#include <base/rpc_client.h>

namespace Genode
{
	/**
 	 * Client-side implementation of the signal session interface
	 */
	struct Signal_session_client : Rpc_client<Signal_session>
	{
		/**
		 * Constructor
		 *
		 * \param s  targeted signal session
		 */
		explicit Signal_session_client(Signal_session_capability const s)
		: Rpc_client<Signal_session>(s) { }

		/******************************
		 ** Signal_session interface **
		 ******************************/

		Signal_receiver_capability alloc_receiver()
		{ return call<Rpc_alloc_receiver>(); }

		Signal_context_capability
		alloc_context(Signal_receiver_capability const r,
		              unsigned const imprint) {
			return call<Rpc_alloc_context>(r, imprint); }

		void free_receiver(Signal_receiver_capability cap) {
			call<Rpc_free_receiver>(cap); }

		void free_context(Signal_context_capability cap) {
			call<Rpc_free_context>(cap); }
	};
}

#endif /* _INCLUDE__SIGNAL_SESSION__CLIENT_H_ */
