/*
 * \brief  Signal-source client interface
 * \author Norman Feske
 * \date   2010-02-03
 *
 * See documentation in 'signal_session/source.h'.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__SOURCE_CLIENT_H_
#define _INCLUDE__SIGNAL_SESSION__SOURCE_CLIENT_H_

#include <signal_session/source.h>
#include <base/rpc_client.h>

namespace Genode { struct Signal_source_client; }


struct Genode::Signal_source_client : Rpc_client<Signal_source>
{
	Signal_source_client(Signal_source_capability signal_source)
	: Rpc_client<Signal_source>(signal_source) { }

	Signal wait_for_signal() override { return call<Rpc_wait_for_signal>(); }
};

#endif /* _INCLUDE__SIGNAL_SESSION__SOURCE_CLIENT_H_ */
