/*
 * \brief  Dummy implementation of the signal-receiver API
 * \author Norman Feske
 * \date   2017-05-11
 *
 * Core receives no signals. Therefore, we can leave the signal receiver
 * blank.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/globals.h>

/* core-local includes */
#include <assertion.h>

using namespace Genode;

Signal_receiver::Signal_receiver() { }

void Signal_receiver::_platform_destructor()                      { }
void Signal_receiver::_platform_begin_dissolve (Signal_context *) { }
void Signal_receiver::_platform_finish_dissolve(Signal_context *) { }

void Signal_receiver::unblock_signal_waiter(Rpc_entrypoint &) { ASSERT_NEVER_CALLED; }

typedef Signal_context_capability Sigh_cap;

Sigh_cap Signal_receiver::manage(Signal_context *) { ASSERT_NEVER_CALLED; }

void Signal_receiver::block_for_signal()
{
	/*
	 * Called by 'entrypoint.cc' after leaving the 'Rpc_construct' RPC call.
	 * This happens in particular when the blocking for the reply for the
	 * 'Rpc_construct' call is canceled by an incoming SIGCHLD signal, which
	 * occurs whenever a child component exits.
	 */
	sleep_forever();
}

void Signal_receiver::local_submit(Signal::Data) { ASSERT_NEVER_CALLED; }
