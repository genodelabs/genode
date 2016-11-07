/*
 * \brief  seL4-specific signal source RPC interface
 * \author Alexander Boettcher
 * \date   2016-07-09
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__SEL4_SIGNAL_SOURCE_H_
#define _INCLUDE__SIGNAL_SOURCE__SEL4_SIGNAL_SOURCE_H_

#include <signal_source/signal_source.h>
#include <base/rpc_server.h>

namespace Genode { struct SeL4_signal_source; }


struct Genode::SeL4_signal_source : Signal_source
{
	GENODE_RPC(Rpc_request_notify_obj, Native_capability, _request_notify);
	GENODE_RPC_INTERFACE_INHERIT(Signal_source, Rpc_request_notify_obj);
};

#endif /* _INCLUDE__SIGNAL_SOURCE__SEL4_SIGNAL_SOURCE_H_ */
