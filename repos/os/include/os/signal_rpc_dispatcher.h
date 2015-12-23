/*
 * \brief  Utility for dispatching signals at at RPC entrypoint
 * \author Norman Feske
 * \date   2013-09-07
 *
 * \deprecated  This header merely exists to maintain API compatibility
 *              to Genode 15.11. Its functionality moved to base/signal.h.
 *              The header will eventually be removed.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_
#define _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_

#include <base/signal.h>

namespace Genode {

	template <typename T, typename EP = Entrypoint>
	struct Signal_rpc_member : Signal_handler<T, EP>
	{
		using Signal_handler<T, EP>::Signal_handler;
	};
}


#endif /* _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_ */
