/*
 * \brief  Fiasco.OC-specific signal source RPC interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2011-04-12
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__FOC_SOURCE_H_
#define _INCLUDE__SIGNAL_SESSION__FOC_SOURCE_H_

#include <base/rpc.h>
#include <signal_session/source.h>

namespace Genode {

	struct Foc_signal_source : Signal_source
	{
		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_request_semaphore, Native_capability, _request_semaphore);

		GENODE_RPC_INTERFACE_INHERIT(Signal_source, Rpc_request_semaphore);
	};
}

#endif /* _INCLUDE__SIGNAL_SESSION__FOC_SOURCE_H_ */
