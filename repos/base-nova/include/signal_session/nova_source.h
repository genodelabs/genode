/*
 * \brief  NOVA-specific signal source RPC interface
 * \author Norman Feske
 * \date   2011-04-12
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__NOVA_SOURCE_H_
#define _INCLUDE__SIGNAL_SESSION__NOVA_SOURCE_H_

#include <base/rpc.h>
#include <signal_session/source.h>

namespace Genode {

	struct Nova_signal_source : Signal_source
	{
		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_register_semaphore, void, _register_semaphore,
		           Native_capability const &);

		GENODE_RPC_INTERFACE_INHERIT(Signal_source, Rpc_register_semaphore);
	};
}

#endif /* _INCLUDE__SIGNAL_SESSION__NOVA_SOURCE_H_ */
