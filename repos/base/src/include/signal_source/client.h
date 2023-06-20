/*
 * \brief  Client-side signal-source interface
 * \author Norman Feske
 * \date   2016-01-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__CLIENT_H_
#define _INCLUDE__SIGNAL_SOURCE__CLIENT_H_

#include <base/rpc_client.h>
#include <pd_session/pd_session.h>
#include <cpu_session/cpu_session.h>
#include <signal_source/signal_source.h>

namespace Genode { class Signal_source_client; }

struct Genode::Signal_source_client : Rpc_client<Signal_source>
{
	Signal_source_client(Cpu_session &, Capability<Signal_source> signal_source)
	: Rpc_client<Signal_source>(signal_source) { }

	Signal wait_for_signal() override { return call<Rpc_wait_for_signal>(); }
};

#endif /* _INCLUDE__SIGNAL_SOURCE__CLIENT_H_ */
