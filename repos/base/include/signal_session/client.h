/*
 * \brief  Client-side signal session interface
 * \author Norman Feske
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__CLIENT_H_
#define _INCLUDE__SIGNAL_SESSION__CLIENT_H_

#include <signal_session/capability.h>
#include <signal_session/signal_session.h>
#include <base/rpc_client.h>
#include <signal_session/source_client.h>

namespace Genode { struct Signal_session_client; }


struct Genode::Signal_session_client : Rpc_client<Signal_session>
{
	explicit Signal_session_client(Signal_session_capability session)
	: Rpc_client<Signal_session>(session) { }

	Signal_source_capability signal_source() override {
		return call<Rpc_signal_source>(); }

	Signal_context_capability alloc_context(long imprint) override {
		return call<Rpc_alloc_context>(imprint); }

	void free_context(Signal_context_capability cap) override {
		call<Rpc_free_context>(cap); }

	void submit(Signal_context_capability receiver, unsigned cnt = 1) override {
		call<Rpc_submit>(receiver, cnt); }
};

#endif /* _INCLUDE__CAP_SESSION__CLIENT_H_ */
