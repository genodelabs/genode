/*
 * \brief  Signal-source server interface
 * \author Norman Feske
 * \date   2010-02-03
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__RPC_OBJECT_H_
#define _INCLUDE__SIGNAL_SOURCE__RPC_OBJECT_H_

#include <base/rpc_server.h>
#include <signal_source/foc_signal_source.h>

namespace Genode { struct Signal_source_rpc_object; }


struct Genode::Signal_source_rpc_object : Rpc_object<Foc_signal_source,
                                                     Signal_source_rpc_object>
{
	protected:

		Native_capability _blocking_semaphore;

	public:

		Signal_source_rpc_object(Native_capability cap)
		: _blocking_semaphore(cap) {}

		Native_capability _request_semaphore() { return _blocking_semaphore; }
};

#endif /* _INCLUDE__SIGNAL_SOURCE__RPC_OBJECT_H_ */
