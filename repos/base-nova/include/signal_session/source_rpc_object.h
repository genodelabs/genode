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
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_
#define _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_

#include <base/rpc_server.h>
#include <signal_session/nova_source.h>

namespace Genode { struct Signal_source_rpc_object; }

struct Genode::Signal_source_rpc_object : Rpc_object<Nova_signal_source,
                                                     Signal_source_rpc_object>
{
	public:

		Native_capability _blocking_semaphore;

	public:

		void _register_semaphore(Native_capability const &cap)
		{
			if (_blocking_semaphore.valid())
				PWRN("overwritting blocking signal semaphore !!!");

			_blocking_semaphore = cap;
		}

		Signal_source_rpc_object() {}
};

#endif /* _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_ */
