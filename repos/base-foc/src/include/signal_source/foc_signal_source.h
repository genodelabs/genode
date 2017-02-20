/*
 * \brief  Fiasco.OC-specific signal source RPC interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2011-04-12
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__FOC_SIGNAL_SOURCE_H_
#define _INCLUDE__SIGNAL_SOURCE__FOC_SIGNAL_SOURCE_H_

#include <signal_source/signal_source.h>
#include <base/rpc_server.h>

namespace Genode { struct Foc_signal_source; }


struct Genode::Foc_signal_source : Signal_source
{
	GENODE_RPC(Rpc_request_semaphore, Native_capability, _request_semaphore);
	GENODE_RPC_INTERFACE_INHERIT(Signal_source, Rpc_request_semaphore);
};

#endif /* _INCLUDE__SIGNAL_SOURCE__FOC_SIGNAL_SOURCE_H_ */
