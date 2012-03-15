/*
 * \brief  Signal-source server interface
 * \author Norman Feske
 * \date   2010-02-03
 *
 * This file is only included by 'signal_session/server.h' and relies on the
 * headers included there. No include guards are needed. It is a separate
 * header file to make it easily replaceable by a platform-specific
 * implementation.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_
#define _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_

#include <base/rpc_server.h>
#include <signal_session/foc_source.h>

namespace Genode {

	struct Signal_source_rpc_object : Rpc_object<Foc_signal_source, Signal_source_rpc_object>
	{
		protected:

			Native_capability _blocking_semaphore;

		public:

			Signal_source_rpc_object(Native_capability cap)
			: _blocking_semaphore(cap) {}

			Native_capability _request_semaphore() { return _blocking_semaphore; }
	};
}

#endif /* _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_ */
